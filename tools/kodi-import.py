#!/usr/bin/env python3
"""Unprivileged Kodi snapshot/import. The desktop source is always read-only."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import stat
import sys
import tempfile
import uuid
from xml.etree import ElementTree

MAX_BYTES=4*1024**3
MAX_FILES=20000
MAX_JSON=8*1024**2
VOLATILE={'temp','cache'}
stopping=False


def check_stop():
    if stopping:raise RuntimeError('Kodi import cancelled; saved configuration is preserved.')


def safe(path):
    path=Path(path)
    if not path.is_absolute() or path.resolve()!=path:raise ValueError('Kodi import paths must be ordinary local directories, without links.')
    return path


def directory(path):
    safe(path)
    path.mkdir(parents=True,exist_ok=True,mode=0o700)
    info=path.stat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid!=os.getuid():
        raise ValueError('Kodi import storage must be owned by this user and protected from other writers.')
    # Qt may have created this app-owned directory under a group-writable umask.
    # Harden only destination/backup storage; source inventory never calls here.
    if info.st_mode&0o077:path.chmod(0o700)


def reject_mounts(root,mountinfo=Path('/proc/self/mountinfo')):
    for line in mountinfo.read_text().splitlines():
        fields=line.split()
        if len(fields)<6:raise ValueError('Could not verify Kodi snapshot mount boundaries.')
        point=Path(re.sub(r'\\([0-7]{3})',lambda match:chr(int(match.group(1),8)),fields[4]))
        if point==root or root in point.parents:raise ValueError('Kodi snapshots refuse mounted/shared subdirectories.')
        if point in root.parents:
            separator=fields.index('-')
            if fields[separator+1] not in {'ext2','ext3','ext4','xfs','btrfs','tmpfs','overlay','zfs'}:
                raise ValueError('Kodi import storage must not use shared or remote mounts.')


def sync_directory(path):
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)


def atomic_json(path,value):
    fd,name=tempfile.mkstemp(prefix='.receipt-',dir=path.parent)
    try:
        with os.fdopen(fd,'w') as stream:json.dump(value,stream,sort_keys=True);stream.flush();os.fsync(stream.fileno())
        os.replace(name,path);sync_directory(path.parent)
    finally:Path(name).unlink(missing_ok=True)


def read_json(path):
    safe(path)
    if not path.is_file() or path.stat().st_uid!=os.getuid() or path.stat().st_size>MAX_JSON:
        raise ValueError('Invalid Kodi import receipt.')
    value=json.loads(path.read_text())
    if not isinstance(value,dict) or value.get('schemaVersion')!=1:raise ValueError('Unsupported Kodi import receipt.')
    return value


def inventory(root):
    """No links/devices/mount traversal; hashes bind every preserved byte."""
    safe(root)
    reject_mounts(root)
    if not root.is_dir():raise ValueError('No existing Kodi configuration was found.')
    if root.stat().st_uid!=os.getuid():raise ValueError('Kodi configuration must belong to this user.')
    result={};total=0;device=root.stat().st_dev
    for parent,dirs,files in os.walk(root,followlinks=False):
        check_stop();base=Path(parent)
        if base==root:dirs[:]=[name for name in dirs if name not in VOLATILE]
        if base==root/'userdata':dirs[:]=[name for name in dirs if name!='isolde']
        for name in sorted(dirs+files):
            path=base/name;relative=path.relative_to(root).as_posix();info=path.lstat()
            if (len(result)>=MAX_FILES or len(relative.encode())>4096 or info.st_uid!=os.getuid()
                    or info.st_dev!=device or path.is_symlink() or (not stat.S_ISDIR(info.st_mode) and not stat.S_ISREG(info.st_mode))):
                raise ValueError('Kodi snapshot refuses redirected, foreign, special or excessive entries.')
            if stat.S_ISDIR(info.st_mode):result[relative]={'type':'directory'};continue
            total+=info.st_size
            if total>MAX_BYTES:raise ValueError('Kodi configuration exceeds the supported 4 GiB snapshot limit.')
            digest=hashlib.sha256()
            fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
            with os.fdopen(fd,'rb') as stream:
                opened=os.fstat(stream.fileno())
                if not stat.S_ISREG(opened.st_mode) or (opened.st_ino,opened.st_dev)!=(info.st_ino,info.st_dev):raise ValueError('Kodi configuration changed during snapshot.')
                count=0
                while data:=stream.read(1024**2):
                    check_stop();count+=len(data)
                    if count>info.st_size:raise ValueError('Kodi configuration changed during snapshot.')
                    digest.update(data)
                if count!=info.st_size:raise ValueError('Kodi configuration changed during snapshot.')
            result[relative]={'type':'file','bytes':count,'sha256':digest.hexdigest(),'executable':bool(info.st_mode&0o100)}
    return result


def snapshot(source,destination):
    before=inventory(source);directory(destination)
    for relative,entry in before.items():
        check_stop();target=destination/relative
        if entry['type']=='directory':directory(target);continue
        directory(target.parent)
        source_fd=os.open(source/relative,os.O_RDONLY|os.O_NOFOLLOW)
        target_fd=os.open(target,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o700 if entry['executable'] else 0o600)
        with os.fdopen(source_fd,'rb') as incoming,os.fdopen(target_fd,'wb') as outgoing:
            copied=0
            while data:=incoming.read(1024**2):
                check_stop();copied+=len(data)
                if copied>entry['bytes']:raise ValueError('Close the original Kodi application before taking a snapshot.')
                outgoing.write(data)
            outgoing.flush();os.fsync(outgoing.fileno())
    if inventory(destination)!=before or inventory(source)!=before:
        raise ValueError('Kodi configuration changed. Close Kodi and retry the snapshot.')
    for parent,_,_ in os.walk(destination,topdown=False):sync_directory(Path(parent))
    return before


class Import:
    def __init__(self,profile,source,config,data,fault=lambda point:None):
        if profile!='default' and (not re.fullmatch(r'[a-f0-9-]{36}',profile) or str(uuid.UUID(profile))!=profile):raise ValueError('Invalid Kodi import profile.')
        self.profile=profile;self.source=safe(Path(source));self.config=safe(Path(config));self.data=safe(Path(data));self.fault=fault
        self.base=self.config/'kodi-import-backups'/profile
        self.target=self.data/'native-profiles'/profile/'kodi/home/.kodi'
        self.receipt=self.base/'receipt.json';self.journal=self.base/'transaction.json'
        if any(left==right or left in right.parents or right in left.parents for left,right in ((self.source,self.target),(self.source,self.base),(self.base,self.target))):
            raise ValueError('Desktop and Isolde Kodi configurations must be separate.')

    def ensure_storage(self):
        reject_mounts(self.config);reject_mounts(self.data)
        directory(self.config);directory(self.data);directory(self.base);directory(self.target.parent)

    def load(self):
        if not self.receipt.exists():return None
        value=read_json(self.receipt)
        if value.get('profile')!=self.profile or value.get('source')!=str(self.source) or value.get('target')!=str(self.target):raise ValueError('Kodi snapshot belongs to another profile or source.')
        token=value.get('token','')
        if not isinstance(token,str) or str(uuid.UUID(token))!=token:raise ValueError('Invalid Kodi snapshot identity.')
        return value

    def backup_path(self,state):return self.base/state['token']

    def recover(self):
        if not self.journal.exists():return
        record=read_json(self.journal);token=record.get('token','')
        if not isinstance(token,str) or str(uuid.UUID(token))!=token or record.get('profile')!=self.profile:raise ValueError('Invalid Kodi recovery identity.')
        new=self.target.parent/('.kodi-new-'+token);old=self.target.parent/('.kodi-old-'+token)
        state=self.load();committed=state and state.get('operationToken')==token
        if committed:
            if old.exists():self.remove(old)
            if new.exists():self.remove(new)
        else:
            if old.exists():
                if self.target.exists():self.remove(self.target)
                os.rename(old,self.target)
            elif not record.get('hadTarget') and self.target.exists() and not new.exists():self.remove(self.target)
            if new.exists():self.remove(new)
        sync_directory(self.target.parent);self.journal.unlink();sync_directory(self.base)

    def remove(self,path):
        safe(path)
        reject_mounts(path)
        allowed=(path==self.target or (path.parent==self.target.parent and re.fullmatch(r'\.kodi-(new|old)-[a-f0-9-]{36}',path.name))
                 or (path.parent==self.base and re.fullmatch(r'[a-f0-9-]{36}',path.name)))
        if not allowed:raise ValueError('Refusing cleanup outside private Kodi transaction storage.')
        device=path.stat().st_dev;count=0
        for parent,dirs,files in os.walk(path,followlinks=False):
            for name in dirs+files:
                entry=Path(parent)/name;info=entry.lstat();count+=1
                if count>MAX_FILES or info.st_uid!=os.getuid() or info.st_dev!=device or entry.is_mount():
                    raise ValueError('Refusing cleanup of foreign or mounted Kodi transaction data.')
        if not shutil.rmtree.avoids_symlink_attacks:raise RuntimeError('Safe Kodi cleanup is unavailable on this platform.')
        shutil.rmtree(path)

    def scan(self,root=None):
        root=root or self.source
        entries=inventory(root);skin='default';sources=0
        def xml(path):
            if path.stat().st_size>4*1024**2:raise ValueError('Kodi configuration XML exceeds the supported 4 MiB limit.')
            raw=path.read_bytes()
            if b'<!DOCTYPE' in raw.upper() or b'<!ENTITY' in raw.upper():raise ValueError('Kodi configuration XML must not contain document entities.')
            return ElementTree.fromstring(raw)
        gui=root/'userdata/guisettings.xml'
        if gui.is_file():
            tree=xml(gui)
            skin=next((node.text for node in tree.iter('setting') if node.get('id')=='lookandfeel.skin' and node.text),'default')
        if not re.fullmatch(r'[A-Za-z0-9_.-]{1,128}',skin):raise ValueError('Invalid Kodi skin identity.')
        source_file=root/'userdata/sources.xml'
        if source_file.is_file():sources=len(xml(source_file).findall('.//source'))
        addons=sum(entry['type']=='directory' and name.startswith('addons/') and name.count('/')==1 for name,entry in entries.items())
        databases=[name for name in entries if re.fullmatch(r'userdata/Database/MyVideos\d*\.db',name)]
        summary=f'Skin: {skin}\nMedia sources: {sources}\nUser add-ons: {addons}\nWatch/resume databases: {len(databases)}\nPreserved files: {sum(e["type"]=="file" for e in entries.values())}\nOriginal desktop configuration remains unchanged. Temporary logs, cache and generated Isolde connection data are excluded.'
        return summary

    def backup(self):
        self.ensure_storage();self.recover()
        if self.load() and self.load().get('imported'):raise ValueError('Roll back the current import before replacing its recovery snapshot.')
        token=str(uuid.uuid4());folder=self.base/token
        try:
            directory(folder);source=snapshot(self.source,folder/'desktop')
            had=self.target.exists();previous=snapshot(self.target,folder/'previous') if had else None
            value=dict(schemaVersion=1,profile=self.profile,source=str(self.source),target=str(self.target),token=token,sourceEntries=source,previousEntries=previous,hadTarget=had,imported=False)
            check_stop();self.fault('before-backup-receipt');check_stop();atomic_json(self.receipt,value)
        except BaseException:
            if folder.exists():self.remove(folder)
            raise

    def replace(self,source,entries,state,imported):
        self.ensure_storage();self.recover();token=str(uuid.uuid4())
        new=self.target.parent/('.kodi-new-'+token);old=self.target.parent/('.kodi-old-'+token)
        had=self.target.exists()
        if source is not None and inventory(source)!=entries:raise ValueError('Kodi snapshot failed its checksum verification.')
        try:
            if source is not None:snapshot(source,new)
            atomic_json(self.journal,dict(schemaVersion=1,profile=self.profile,token=token,hadTarget=had))
            self.fault('prepared')
            check_stop()
            if had:os.rename(self.target,old)
            self.fault('old-renamed')
            check_stop()
            if source is not None:os.rename(new,self.target)
            sync_directory(self.target.parent);self.fault('target-replaced')
            check_stop()
            state=dict(state,imported=imported,operationToken=token)
            self.fault('before-import-receipt');check_stop();atomic_json(self.receipt,state)
            self.fault('after-import-receipt')
        except BaseException:
            self.recover()
            if new.exists():self.remove(new)
            raise
        self.recover()

    def install(self):
        self.recover();state=self.load()
        if not state:raise ValueError('Create and review a complete backup before importing.')
        if state.get('imported'):raise ValueError('This snapshot is already imported.')
        current=inventory(self.target) if self.target.exists() else None
        if current!=state['previousEntries']:raise ValueError('Isolde Kodi changed after backup. Create a new backup before importing.')
        self.replace(self.backup_path(state)/'desktop',state['sourceEntries'],state,True)

    def rollback(self):
        self.recover();state=self.load()
        if not state or not state.get('imported'):raise ValueError('No imported configuration is available to roll back.')
        self.replace(self.backup_path(state)/'previous' if state['hadTarget'] else None,state['previousEntries'],state,False)

    def status(self,preview=''):
        state=self.load();pending=self.journal.exists()
        return dict(ok=True,backupAvailable=bool(state),backupPath=str(self.backup_path(state)) if state else '',canRollback=bool(state and state.get('imported') and not pending),canImport=not pending and self.source.is_dir(),previewText=preview,conflicts=['Stop playback before import. Import replaces this Isolde profile only.'] if state and not state.get('imported') else [],importStatus='Interrupted import needs recovery. Choose Scan.' if pending else 'Imported into this Isolde profile; desktop Kodi is unchanged.' if state and state.get('imported') else 'Snapshot ready for preview and import.' if state else 'Scan the existing desktop Kodi configuration to begin.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',required=True);parser.add_argument('--config-root',required=True,type=Path);parser.add_argument('--data-root',required=True,type=Path)
    parser.add_argument('action',choices=('status','scan','preview','backup','import','rollback','recover'));args=parser.parse_args()
    if sys.platform!='linux' or os.getuid()==0:raise ValueError('Kodi import requires the ordinary Linux user session.')
    operation=Import(args.profile,Path.home()/'.kodi',args.config_root,args.data_root)
    lock=None
    if args.action!='status':
        operation.ensure_storage()
        fd=os.open(operation.base/'operation.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
        lock=os.fdopen(fd,'w')
        if os.fstat(fd).st_uid!=os.getuid():raise ValueError('Invalid Kodi import operation lock.')
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise RuntimeError('Another Kodi import operation is active. Wait and retry.')
    preview=''
    if args.action=='scan':operation.recover();preview=operation.scan()
    elif args.action=='preview':
        operation.recover();state=operation.load()
        preview=operation.scan(operation.backup_path(state)/'desktop' if state else None)
    elif args.action=='backup':operation.backup();preview=operation.scan()
    elif args.action=='import':operation.install()
    elif args.action=='rollback':operation.rollback()
    elif args.action=='recover':operation.recover()
    print(json.dumps(operation.status(preview)),flush=True)


if __name__=='__main__':
    def stop(*unused):
        global stopping
        stopping=True
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    try:main()
    except Exception as error:print(json.dumps(dict(ok=False,error=str(error))),flush=True);sys.exit(1)
