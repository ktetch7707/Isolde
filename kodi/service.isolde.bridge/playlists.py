"""Kodi-owned smart playlists created from validated Isolde library views."""
import os
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET


class Playlists:
    def __init__(self, profile, managed=False):
        self.profile = Path(profile).absolute()
        self.managed = managed

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
            return directory
        except Exception:
            os.close(directory)
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

    def delete(self, params):
        if not self.managed or set(params) != {"name"}:
            raise ValueError("Managed Kodi playlist deletion required")
        name = params.get("name")
        if not playlist_name(name):raise ValueError("Invalid playlist name")
        directory=self._playlist_dir()
        try:
            os.unlink(name,dir_fd=directory);os.fsync(directory)
        except FileNotFoundError:
            raise ValueError("Playlist not found")
        finally:
            os.close(directory)
        return {"deleted":True,"name":name}

    def rename(self, params):
        if not self.managed or set(params) != {"name","newName"}:
            raise ValueError("Managed Kodi playlist rename required")
        name, new_name = params.get("name"), params.get("newName")
        if not playlist_name(name) or not playlist_name(new_name):
            raise ValueError("Invalid playlist name")
        if name == new_name:
            return {"renamed":True,"name":name,"newName":new_name}
        directory=self._playlist_dir()
        temporary=".isolde-rename-"+str(uuid.uuid4())
        try:
            os.link(name,temporary,src_dir_fd=directory,dst_dir_fd=directory,follow_symlinks=False)
            os.unlink(name,dir_fd=directory);os.fsync(directory)
            os.link(temporary,new_name,src_dir_fd=directory,dst_dir_fd=directory,follow_symlinks=False)
            os.unlink(temporary,dir_fd=directory);os.fsync(directory)
        except FileExistsError:
            try:os.unlink(temporary,dir_fd=directory)
            except FileNotFoundError:pass
            raise ValueError("A playlist with the new name already exists")
        except FileNotFoundError:
            try:os.unlink(temporary,dir_fd=directory)
            except FileNotFoundError:pass
            raise ValueError("Playlist not found")
        finally:
            os.close(directory)
        return {"renamed":True,"name":name,"newName":new_name}

    def reorderPlaylist(self, params):
        if not self.managed or set(params) != {"name","order"}:
            raise ValueError("Managed Kodi playlist reorder required")
        name = params.get("name")
        order = params.get("order")
        if not playlist_name(name):raise ValueError("Invalid playlist name")
        if (not isinstance(order,list) or not order
            or not all(isinstance(i,int) and i>=0 for i in order)
            or len(set(order))!=len(order)):
            raise ValueError("Order must be a non-empty list of unique non-negative indices")
        directory=self._playlist_dir()
        try:
            fd=os.open(name,os.O_RDONLY|os.O_NOFOLLOW,dir_fd=directory)
            try:
                raw=os.read(fd,1024*1024)
            finally:
                os.close(fd)
            try:
                text=raw.decode("utf-8")
            except UnicodeDecodeError:
                raise ValueError("Playlist is not valid UTF-8")
            # Smart playlists (.xsp) are rule-based; reorder applies to
            # explicit member playlists (.m3u/.m3u8) only.
            if name.endswith(".xsp"):
                raise ValueError("Smart playlists are rule-based; member order is set by the rule.")
            if not (name.endswith(".m3u") or name.endswith(".m3u8")):
                raise ValueError("Only .m3u and .m3u8 playlists can be reordered.")
            lines=text.split("\n")
            members=[line for line in lines if line and not line.startswith("#")]
            if len(order)!=len(members) or set(order)!=set(range(len(members))):
                raise ValueError("Order must include every playlist member exactly once.")
            reordered=[members[i] for i in order]
            out_lines=[]
            member_idx=0
            for line in lines:
                if line and not line.startswith("#"):
                    out_lines.append(reordered[member_idx]);member_idx+=1
                else:
                    out_lines.append(line)
            data="\n".join(out_lines).encode("utf-8")
            temporary=".isolde-reorder-"+str(uuid.uuid4())
            fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=directory)
            try:
                os.write(fd,data);os.fsync(fd)
            finally:
                os.close(fd)
            os.rename(temporary,name,src_dir_fd=directory,dst_dir_fd=directory)
            os.fsync(directory)
        except FileNotFoundError:
            raise ValueError("Playlist not found")
        finally:
            os.close(directory)
        return {"reordered":True,"name":name,"count":len(members)}
