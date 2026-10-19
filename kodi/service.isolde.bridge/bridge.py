"""Bounded local transport; Kodi API access is injected for unit testing.

One request per connection, JSON terminated by newline. No TCP ports, shell
commands, arbitrary Kodi methods, database access or credentials in QML.
"""
import json
import os
from pathlib import Path
import socket
import stat
import struct

MAX_REQUEST = 32768
MAX_RESPONSE = 4 * 1024 * 1024
READ_METHODS = {
    "JSONRPC.Ping", "JSONRPC.Version", "Application.GetProperties",
    "VideoLibrary.GetMovies", "VideoLibrary.GetTVShows", "VideoLibrary.GetSeasons", "VideoLibrary.GetEpisodes",
    "VideoLibrary.GetMovieDetails", "VideoLibrary.GetTVShowDetails",
    "Player.GetActivePlayers", "Player.GetProperties",
}
MOVIE_DETAIL_PROPERTIES = {"title", "year", "plot", "runtime", "resume", "playcount", "art", "genre", "tag"}
TVSHOW_DETAIL_PROPERTIES = {"title", "year", "plot", "episode", "watchedepisodes", "art", "genre", "tag"}


def dispatch(raw, execute, playback=None, artwork=None):
    try:
        request = json.loads(raw)
    except (ValueError, UnicodeDecodeError):
        return {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Invalid JSON"}}
    request_id = request.get("id") if isinstance(request, dict) else None
    def error(code, message):
        return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}
    if not isinstance(request, dict) or request.get("jsonrpc") != "2.0" or type(request_id) is not int:
        return error(-32600, "Invalid request")
    playback_methods = {"Isolde.PlayMovie", "Isolde.PlayEpisode", "Isolde.PlaybackStatus", "Isolde.PlaybackControl"}
    if not isinstance(request.get("method"), str) or request.get("method") not in READ_METHODS | playback_methods | {"Isolde.GetArtwork", "VideoLibrary.Scan", "XBMC.GetInfoBooleans", "Isolde.QuitIfIdle"}:
        return error(-32601, "Method is not enabled by the Isolde bridge")
    params = request.get("params", {})
    if not isinstance(params, dict):
        return error(-32602, "Parameters must be an object")
    if request["method"] == "Isolde.GetArtwork":
        try:
            if artwork is None:
                raise ValueError("Artwork unavailable")
            return {"jsonrpc": "2.0", "id": request_id, "result": artwork.get(params)}
        except Exception:
            return error(-32000, "Local Kodi artwork unavailable")
    if request["method"] in ("VideoLibrary.GetMovieDetails", "VideoLibrary.GetTVShowDetails"):
        identity = "movieid" if request["method"] == "VideoLibrary.GetMovieDetails" else "tvshowid"
        allowed_properties = MOVIE_DETAIL_PROPERTIES if identity == "movieid" else TVSHOW_DETAIL_PROPERTIES
        if (set(params) != {identity, "properties"} or type(params.get(identity)) is not int or params[identity] < 1
            or not isinstance(params.get("properties"), list) or not params["properties"]
            or len(params["properties"]) > len(allowed_properties)
            or any(not isinstance(value, str) or value not in allowed_properties for value in params["properties"])):
            return error(-32602, "Library details require a valid ID and supported properties")
    if request["method"] in playback_methods:
        try:
            if playback is None:
                raise ValueError("Managed playback unavailable")
            return {"jsonrpc": "2.0", "id": request_id, "result": playback.handle(request["method"], params)}
        except Exception:
            return error(-32000, "Playback unavailable or rejected; check local media access and player state")
    if request["method"] == "Isolde.QuitIfIdle":
        if params:
            return error(-32602, "Quit accepts no parameters")
        try:
            def query(method, parameters):
                reply = json.loads(execute(json.dumps({"jsonrpc": "2.0", "id": request_id,
                                                      "method": method, "params": parameters})))
                if "error" in reply:
                    raise ValueError("Unavailable")
                return reply["result"]
            players = query("Player.GetActivePlayers", {})
            scans = query("XBMC.GetInfoBooleans", {"booleans": ["Library.IsScanningVideo", "Library.IsScanningMusic"]})
            if not isinstance(players, list) or not isinstance(scans, dict):
                raise ValueError("Invalid status")
            if any(type(scans.get(key)) is not bool for key in ("Library.IsScanningVideo", "Library.IsScanningMusic")):
                raise ValueError("Invalid status")
            if players or any(scans.values()):
                return error(-32000, "Stop playback and wait for library scans before switching modes")
            return {"jsonrpc": "2.0", "id": request_id, "result": query("Application.Quit", {})}
        except Exception:
            return error(-32603, "Cannot confirm Kodi is idle; mode was not changed")
    if request["method"] == "VideoLibrary.Scan":
        if params:
            return error(-32602, "Scan uses only this profile's configured sources")
        params = {"showdialogs": False}
    elif request["method"] == "XBMC.GetInfoBooleans":
        if params != {"booleans": ["Library.IsScanningVideo"]}:
            return error(-32602, "Only video scan status is available")
    elif request["method"].startswith("VideoLibrary.") and request["method"] not in (
        "VideoLibrary.GetMovieDetails", "VideoLibrary.GetTVShowDetails"):
        params = dict(params)
        for key in ("tvshowid", "season"):
            if key in params and (type(params[key]) is not int or params[key] < (0 if key == "season" else 1)):
                return error(-32602, "Invalid show or season identity")
        if request["method"] == "VideoLibrary.GetSeasons" and "tvshowid" not in params:
            return error(-32602, "A show is required for seasons")
        if "filter" in params:
            rule = params["filter"]
            if not isinstance(rule, dict) or set(rule) != {"field", "operator", "value"}:
                return error(-32602, "Only local title, tag, genre and progress filters are supported")
            valid = ((rule["field"], rule["operator"]) in (("title", "contains"), ("tag", "is"), ("genre", "is"), ("inprogress", "true")))
            if not valid or not isinstance(rule["value"], str) or len(rule["value"]) > 160:
                return error(-32602, "Invalid local library filter")
            if request["method"] == "VideoLibrary.GetSeasons" or (request["method"] == "VideoLibrary.GetTVShows" and rule["field"] != "title"):
                return error(-32602, "This filter is not supported for this view")
        limits = params.get("limits", {"start": 0, "end": 50})
        if (not isinstance(limits, dict) or type(limits.get("start", 0)) is not int
            or type(limits.get("end")) is not int or limits.get("start", 0) < 0
            or not 0 < limits["end"] - limits.get("start", 0) <= 100):
            return error(-32602, "Library requests must use pages of 1 to 100 items")
        params["limits"] = limits
    try:
        return json.loads(execute(json.dumps({"jsonrpc": "2.0", "id": request_id,
                                            "method": request["method"], "params": params})))
    except Exception:
        # Do not leak Kodi paths, addon details, or exception contents.
        return error(-32603, "Kodi could not process the request")


class LocalBridge:
    def __init__(self, directory, execute, playback=None, artwork=None):
        self.directory = Path(directory)
        self.execute = execute
        self.playback = playback
        self.artwork = artwork
        self.listener = None
        self.socket_path = self.directory / "rpc.sock"
        self.identity = None

    def start(self):
        # Socket resides inside the already isolated Kodi home, with a short
        # filename so Linux's UNIX socket pathname limit is not exceeded.
        if self.directory.is_symlink():
            raise RuntimeError("Bridge directory must not be a symlink")
        self.directory.mkdir(mode=0o700, parents=True, exist_ok=True)
        if self.directory.stat().st_uid != os.getuid():
            raise RuntimeError("Bridge directory has a different owner")
        os.chmod(self.directory, 0o700)
        if self.socket_path.exists() or self.socket_path.is_symlink():
            existing = self.socket_path.lstat()
            if not stat.S_ISSOCK(existing.st_mode) or existing.st_uid != os.getuid():
                raise RuntimeError("Refusing to replace an unrelated bridge path")
            with socket.socket(socket.AF_UNIX) as probe:
                try:
                    probe.connect(str(self.socket_path))
                except ConnectionRefusedError:
                    self.socket_path.unlink()
                else:
                    raise RuntimeError("A bridge is already running")
        self.listener = socket.socket(socket.AF_UNIX)
        self.listener.bind(str(self.socket_path))
        os.chmod(self.socket_path, 0o600)
        self.identity = self.socket_path.stat().st_ino
        self.listener.listen(4)
        self.listener.settimeout(0.25)

    def poll(self):
        try:
            client, _ = self.listener.accept()
        except socket.timeout:
            return
        with client:
            client.settimeout(1)
            _, uid, _ = struct.unpack("3i", client.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
            if uid != os.getuid():
                return
            try:
                raw = b""
                while b"\n" not in raw and len(raw) <= MAX_REQUEST:
                    part = client.recv(min(4096, MAX_REQUEST + 1 - len(raw)))
                    if not part:
                        return
                    raw += part
                if len(raw) > MAX_REQUEST or not raw.endswith(b"\n"):
                    return
                response = json.dumps(dispatch(raw, self.execute, self.playback, self.artwork)).encode() + b"\n"
                if len(response) <= MAX_RESPONSE:
                    client.sendall(response)
            except (OSError, ValueError):
                pass

    def close(self):
        if self.listener:
            self.listener.close()
        if self.identity and self.socket_path.exists() and self.socket_path.lstat().st_ino == self.identity:
            self.socket_path.unlink()
