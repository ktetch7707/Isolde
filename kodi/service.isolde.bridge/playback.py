"""Managed playback only: library IDs, local readable files, owned video player."""
import json
import os
import time


class Playback:
    def __init__(self, execute, builtin, managed=False, clock=time.monotonic):
        self.execute, self.builtin, self.managed, self.clock = execute, builtin, managed, clock
        self.movie = None
        self.media_type = "movie"
        self.state = "idle"
        self.deadline = 0
        self.seen = False
        self.home = False
        self.pending_stop = None

    def rpc(self, method, params=None):
        reply = json.loads(self.execute(json.dumps({"jsonrpc": "2.0", "id": 1,
                                                   "method": method, "params": params or {}})))
        if "error" in reply or "result" not in reply:
            raise ValueError("Kodi rejected playback operation")
        return reply["result"]

    def handle(self, method, params):
        if not self.managed:
            raise ValueError("Playback requires Isolde frontend mode")
        if method in ("Isolde.PlayMovie", "Isolde.PlayEpisode", "Isolde.PlaySong", "Isolde.PlayChannel"):
            media_type = "movie" if method == "Isolde.PlayMovie" else "song" if method == "Isolde.PlaySong" else "channel" if method == "Isolde.PlayChannel" else "episode"
            key = media_type + "id"
            if set(params) != {key, "resume"} or type(params[key]) is not int or params[key] < 1 or type(params["resume"]) is not bool:
                raise ValueError("A library ID and explicit resume choice are required")
            if media_type in {"song","channel"} and params["resume"]:
                raise ValueError("Music and live TV do not use video resume")
            if self.movie is not None or self.rpc("Player.GetActivePlayers"):
                raise ValueError("Another playback session is active")
            if media_type == "channel":
                details = self.rpc("PVR.GetChannelDetails", {key:params[key],"properties":["channeltype","locked","hidden"]}).get("channeldetails",{})
                if details.get("channeltype") != "tv" or details.get("locked",False) or details.get("hidden",False):
                    raise ValueError("Choose an available unlocked Kodi TV channel")
            else:
                details = self.rpc("VideoLibrary.GetMovieDetails" if media_type == "movie" else "AudioLibrary.GetSongDetails" if media_type == "song" else "VideoLibrary.GetEpisodeDetails",
                                   {key: params[key], "properties": ["file"]})
                path = details.get(media_type + "details", {}).get("file", "")
                if not isinstance(path, str) or not os.path.isabs(path) or not os.path.isfile(path) or not os.access(path, os.R_OK):
                    raise ValueError("This media is not an available local file")
            if self.rpc("Player.Open", {"item": {key: params[key]}, "options": {"resume": params["resume"]}}) != "OK":
                raise ValueError("Kodi did not accept playback")
            self.movie, self.state, self.seen, self.home = params[key], "starting", False, False
            self.media_type = media_type
            self.pending_stop = None
            self.deadline = self.clock() + 20
            return self.status()
        if method == "Isolde.PlaybackStatus" and not params:
            return self.status()
        if method == "Isolde.PlaybackControl" and set(params) == {"action"}:
            return self.control(params["action"])
        raise ValueError("Unsupported playback request")

    def status(self):
        if self.movie is not None:
            players = self.rpc("Player.GetActivePlayers")
            videos = [p for p in players if p.get("type") == ("audio" if self.media_type == "song" else "video")]
            if videos:
                player = videos[0]["playerid"]
                item = self.rpc("Player.GetItem", {"playerid": player}).get("item", {})
                if item.get("type") != self.media_type or item.get("id") != self.movie:
                    if not self.seen and self.clock() < self.deadline:
                        return {"state": "starting", "home": self.home}
                    # Never stop a different player/item that we do not own.
                    self.movie, self.state = None, "replaced"
                else:
                    props = self.rpc("Player.GetProperties", {"playerid": player, "properties": ["speed"]})
                    if not self.seen:
                        self.builtin("ActivateWindow(visualisation)" if self.media_type == "song" else "ActivateWindow(fullscreenvideo)")
                    self.seen = True
                    self.state = "paused" if props.get("speed") == 0 else "playing"
                    if self.pending_stop is not None:
                        self.home = self.pending_stop == "home"
                        self.pending_stop = None
                        self.rpc("Player.Stop", {"playerid": player})
            elif self.seen or self.clock() >= self.deadline:
                self.movie, self.state = None, "ended" if self.seen else "failed"
                self.builtin("Minimize")
        return {"state": self.state, "home": self.home}

    def control(self, action):
        if action not in ("pause", "stop", "home", "forward", "backward"):
            raise ValueError("Unsupported playback control")
        self.status()
        if self.movie is None:
            return self.status()
        if action in ("stop", "home") and not self.seen:
            self.pending_stop = action
            self.home = action == "home"
            return self.status()
        players = self.rpc("Player.GetActivePlayers")
        for player in players:
            if player.get("type") != ("audio" if self.media_type == "song" else "video"):
                continue
            pid = player["playerid"]
            item = self.rpc("Player.GetItem", {"playerid": pid}).get("item", {})
            if item.get("id") != self.movie or item.get("type") != self.media_type:
                raise ValueError("Playback ownership changed")
            if action in ("stop", "home"):
                self.home = action == "home"
                self.rpc("Player.Stop", {"playerid": pid})
            elif action == "pause":
                self.rpc("Player.PlayPause", {"playerid": pid, "play": "toggle"})
            else:
                self.rpc("Player.Seek", {"playerid": pid, "value": "smallforward" if action == "forward" else "smallbackward"})
        return self.status()
