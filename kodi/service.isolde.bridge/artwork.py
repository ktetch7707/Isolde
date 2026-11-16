"""Read Kodi-owned art; apply explicitly confirmed managed posters through Kodi's API."""
import base64
import json
import os
import stat
import hashlib
import struct
import secrets
import re
from pathlib import Path
from urllib.parse import unquote, quote, urlsplit


class Artwork:
    def __init__(self, execute, thumbnails, open_image=None):
        self.execute = execute
        self.thumbnails = Path(thumbnails).resolve()
        self.open_image = open_image

    def query(self, method, params):
        reply = json.loads(self.execute(json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})))
        if "error" in reply:
            raise ValueError("Kodi art query failed")
        return reply["result"]

    def set_poster(self, params):
        """Publish one bounded managed PNG, then ask Kodi to change its own metadata."""
        kind, ident = params.get("kind"), params.get("id")
        if set(params) != {"kind", "id", "data"} or kind not in {"movie", "tvshow"} or type(ident) is not int or ident < 1:
            raise ValueError("A movie or show identity is required")
        encoded = params["data"]
        if not isinstance(encoded, str) or len(encoded) > 1398104:
            raise ValueError("Poster exceeds the managed limit")
        raw = base64.b64decode(encoded, validate=True)
        if not 33 <= len(raw) <= 1024 * 1024 or raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR":
            raise ValueError("A managed PNG is required")
        width, height = struct.unpack(">II", raw[16:24])
        if not 0 < width <= 4096 or not 0 < height <= 4096 or width * height > 4 * 1024 * 1024:
            raise ValueError("Poster dimensions exceed the limit")
        suffix = "Movie" if kind == "movie" else "TVShow"
        identity = {kind + "id": ident}
        details = self.query("VideoLibrary.Get" + suffix + "Details", dict(identity, properties=["art"]))
        old = details.get(kind + "details", {}).get("art")
        if not isinstance(old, dict):
            raise ValueError("Kodi title no longer exists")
        name = "isolde-" + hashlib.sha256(raw).hexdigest() + ".png"
        directory = os.open(self.thumbnails, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            try:
                descriptor = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
            except FileNotFoundError:
                count = 0
                with os.scandir(directory) as entries:
                    for index, entry in enumerate(entries):
                        if index >= 4096:
                            raise ValueError("Kodi artwork directory exceeds the managed inspection limit")
                        if re.fullmatch(r"isolde-[0-9a-f]{64}\.png", entry.name): count += 1
                        if count >= 256:
                            raise ValueError("Managed poster limit reached; existing artwork is preserved")
                temporary = ".isolde-poster-" + secrets.token_hex(16)
                descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
                try:
                    with os.fdopen(descriptor, "wb") as target:
                        target.write(raw); target.flush(); os.fsync(target.fileno())
                    os.replace(temporary, name, src_dir_fd=directory, dst_dir_fd=directory)
                finally:
                    try: os.unlink(temporary, dir_fd=directory)
                    except FileNotFoundError: pass
            else:
                with os.fdopen(descriptor, "rb") as existing:
                    info = os.fstat(existing.fileno())
                    if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_size != len(raw) or existing.read(len(raw) + 1) != raw:
                        raise ValueError("Managed poster collision")
            os.fsync(directory)
        finally:
            os.close(directory)
        path = str(self.thumbnails / name)
        try:
            if self.query("VideoLibrary.Set" + suffix + "Details", dict(identity, art={"poster": path})) != "OK":
                raise ValueError("Kodi did not acknowledge the poster")
        except Exception:
            # A response can fail after Kodi applied it. Restore through Kodi's API.
            self.query("VideoLibrary.Set" + suffix + "Details", dict(identity, art={"poster": old.get("poster", "")}))
            raise ValueError("Poster update failed; refresh before retrying")
        return {"updated": True}

    def get(self, params):
        kinds = {"movie": "Movie", "tvshow": "TVShow", "season": "Season", "episode": "Episode", "album": "Album", "song": "Song", "channel":"Channel"}
        kind, ident = params.get("kind"), params.get("id")
        if set(params) != {"kind", "id"} or kind not in kinds or type(ident) is not int or ident < 1:
            raise ValueError("A library identity is required")
        namespace = "AudioLibrary." if kind in {"album", "song"} else "PVR." if kind == "channel" else "VideoLibrary."
        details = self.query(namespace + "Get" + kinds[kind] + "Details", {kind + "id": ident, "properties": ["icon"] if kind == "channel" else ["art"]})
        art = details.get(kind + "details", {}).get("art", {})
        if kind == "channel": art = {"thumb":details.get("channeldetails",{}).get("icon","")}
        for name in ("poster", "tvshow.poster", "thumb", "fanart"):
            url = art.get(name, "")
            if not isinstance(url, str) or not url or len(url) > 8192:
                continue
            original = unquote(url[8:-1]) if url.startswith("image://") and url.endswith("/") else url
            paths = []
            # Use Kodi's supported texture API rather than opening its database.
            textures = self.query("Textures.GetTextures", {"properties": ["cachedurl"],
                "filter": {"field": "url", "operator": "is", "value": original}}).get("textures", [])
            for texture in textures[:4]:
                cached = (self.thumbnails / texture.get("cachedurl", "")).resolve()
                if cached.is_relative_to(self.thumbnails):
                    paths.append(cached)
            if os.path.isabs(original):
                paths.append(Path(original))  # Kodi's own sandbox limits media access.
            for path in paths:
                try:
                    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
                    with os.fdopen(descriptor, "rb") as stream:
                        info = os.fstat(stream.fileno())
                        if not stat.S_ISREG(info.st_mode) or info.st_size > 1024 * 1024:
                            continue
                        raw = stream.read(1024 * 1024 + 1)
                    if len(raw) > 1024 * 1024:
                        continue
                    mime = "image/png" if raw.startswith(b"\x89PNG\r\n\x1a\n") else "image/jpeg" if raw.startswith(b"\xff\xd8\xff") else ""
                    if mime:
                        return {"mime": mime, "data": base64.b64encode(raw).decode("ascii")}
                except OSError:
                    continue
            # Kodi's image:// VFS populates its own texture cache. Never fetch
            # an arbitrary caller URL or maintain a parallel Isolde art cache.
            parsed=urlsplit(original)
            if self.open_image and parsed.scheme in ('https','http') and parsed.hostname and not parsed.username and not parsed.password:
                stream=None
                try:
                    stream=self.open_image('image://'+quote(original,safe='')+'/')
                    raw=bytes(stream.readBytes(1024*1024+1)) if 0<stream.size()<=1024*1024 else b''
                    mime='image/png' if raw.startswith(b'\x89PNG\r\n\x1a\n') else 'image/jpeg' if raw.startswith(b'\xff\xd8\xff') else ''
                    if mime and len(raw)<=1024*1024:return {'mime':mime,'data':base64.b64encode(raw).decode('ascii')}
                except (OSError,RuntimeError):
                    pass
                finally:
                    if stream is not None:stream.close()
                # At most one Kodi cache-population attempt per request.
                break
        return {"mime": "", "data": ""}
