"""Kodi-owned smart playlists created from validated Isolde library views."""
import os
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET


class Playlists:
    def __init__(self, profile, managed=False):
        self.profile = Path(profile).absolute()
        self.managed = managed

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
        directory=os.open("/",os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        published=False
        temporary_created=False
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
            with os.scandir(directory) as entries:
                if sum(1 for _,entry in zip(range(128),entries))>=128:raise ValueError("At most 128 saved video playlists")
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
