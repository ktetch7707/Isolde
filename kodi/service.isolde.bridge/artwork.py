"""Read only Kodi-reported local/cached art. Never fetch URLs or populate caches."""
import base64
import json
import os
import stat
from pathlib import Path
from urllib.parse import unquote


class Artwork:
    def __init__(self, execute, thumbnails):
        self.execute = execute
        self.thumbnails = Path(thumbnails).resolve()

    def query(self, method, params):
        reply = json.loads(self.execute(json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})))
        if "error" in reply:
            raise ValueError("Kodi art query failed")
        return reply["result"]

    def get(self, params):
        kinds = {"movie": "Movie", "tvshow": "TVShow", "season": "Season", "episode": "Episode"}
        kind, ident = params.get("kind"), params.get("id")
        if set(params) != {"kind", "id"} or kind not in kinds or type(ident) is not int or ident < 1:
            raise ValueError("A library identity is required")
        details = self.query("VideoLibrary.Get" + kinds[kind] + "Details", {kind + "id": ident, "properties": ["art"]})
        art = details.get(kind + "details", {}).get("art", {})
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
        return {"mime": "", "data": ""}
