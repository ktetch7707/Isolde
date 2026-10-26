#!/usr/bin/env python3
"""Explicit, guest-only freely distributed gameplay fixtures. Never a ROM scraper."""
import hashlib
import io
import itertools
import json
import os
from pathlib import Path
import socket
import stat
import tempfile
import urllib.request
import zipfile

URL="https://store.ppsspp.org/files/webfest.zip"
SHA256="cde1525771bed0413e6de0dd197884be4971b9e127a40b02ad817b8eac7ec595"


def members(data):
    if hashlib.sha256(data).hexdigest()!=SHA256:raise ValueError("Gameplay fixture hash mismatch")
    files={};total=0
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if len(archive.infolist())>1024:raise ValueError("Too many fixture files")
        for entry in archive.infolist():
            path=entry.filename
            if (not path.startswith("webfest/") or "\\" in path or ":" in path or "\x00" in path
                    or any(part in (".","..","") for part in path.rstrip("/").split("/"))
                    or len(path)>512 or len(path.split("/"))>10 or stat.S_ISLNK(entry.external_attr>>16)):
                raise ValueError("Unsafe gameplay fixture archive")
            if entry.is_dir():continue
            total+=entry.file_size
            if total>16*1024**2 or path in files:raise ValueError("Oversized or duplicate fixture file")
            files[path]=archive.read(entry)
    if "webfest/EBOOT.PBP" not in files:raise ValueError("Missing PSP game fixture")
    return files


def fetch():
    if socket.gethostname()!="isolde-test":raise RuntimeError("Gameplay fixture setup is restricted to Isolde-Test")
    parent=Path.home()/"Games/Isolde-Acceptance"
    for ancestor in [parent,*parent.parents]:
        if ancestor.is_symlink():raise ValueError("Redirected fixture directory")
    parent.mkdir(parents=True,exist_ok=True)
    cached=parent/"webfest-1.2.zip"
    if cached.is_symlink():raise ValueError("Redirected fixture archive cache")
    if cached.exists():
        if not cached.is_file() or cached.stat().st_size>2*1024**2:raise ValueError("Invalid fixture archive cache")
        data=cached.read_bytes()
    else:
        request=urllib.request.Request(URL,headers={"User-Agent":"Isolde-test/1"})
        with urllib.request.urlopen(request,timeout=30) as response:
            if response.url!=URL:raise ValueError("Unexpected fixture redirect")
            data=response.read(2*1024**2+1)
    if len(data)>2*1024**2:raise ValueError("Oversized fixture download")
    files=members(data);destination=parent/"webfest-1.2"
    if not cached.exists():
        fd,temporary=tempfile.mkstemp(prefix=".archive-",dir=parent)
        try:
            with os.fdopen(fd,"wb") as output:output.write(data);output.flush();os.fsync(output.fileno())
            os.replace(temporary,cached)
        finally:
            if os.path.exists(temporary):os.unlink(temporary)
    if destination.exists() or destination.is_symlink():
        if destination.is_symlink() or destination.resolve()!=destination:raise ValueError("Redirected existing fixture")
        existing=list(itertools.islice(destination.rglob('*'),2049))
        if len(existing)>2048 or any(path.is_symlink() for path in existing) or {str(path.relative_to(destination)) for path in existing if path.is_file()}!=set(files):
            raise ValueError("Existing fixture contains unexpected files; preserve it and review")
        for path,content in files.items():
            candidate=destination/path
            if candidate.is_symlink() or candidate.resolve()!=candidate or not candidate.is_file() or candidate.read_bytes()!=content:
                raise ValueError("Existing fixture changed; preserve it and review before replacing")
    else:
        with tempfile.TemporaryDirectory(prefix=".webfest-",dir=parent) as temporary:
            stage=Path(temporary)/"game";stage.mkdir()
            for path,content in files.items():
                candidate=stage/path;candidate.parent.mkdir(parents=True,exist_ok=True);candidate.write_bytes(content)
            os.rename(stage,destination)
    return {"name":"Webfest","version":"1.2","platform":"PSP","runtimeId":"ppsspp","root":str(destination/"webfest"),
        "target":"EBOOT.PBP","archiveSha256":SHA256,"source":URL,
        "terms":"https://www.ppsspp.org/docs/reference/homebrew-store-distribution/",
        "gameplayVerified":False,"files":{path:hashlib.sha256(content).hexdigest() for path,content in files.items()}}


if __name__=="__main__":print(json.dumps(fetch(),indent=2))
