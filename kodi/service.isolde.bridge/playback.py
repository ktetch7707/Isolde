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
        self.scheduled_offset = None
        self.opened_at = 0
        self.failure_reason = ""
        self.control_error = ""
        self.can_pause = self.can_seek = False
        self.natural_end = False
        self.stop_requested = False
        self.missing_since = None
        self.generation = 0

    def rpc(self, method, params=None):
        reply = json.loads(self.execute(json.dumps({"jsonrpc": "2.0", "id": 1,
                                                   "method": method, "params": params or {}})))
        if "error" in reply or "result" not in reply:
            raise ValueError("Kodi rejected playback operation")
        return reply["result"]

    def pause_available(self):
        # Kodi exposes CanPause through its read-only GUI boolean, not a
        # Player.GetProperties field. Unknown capability must not break status.
        try:
            return self.rpc("XBMC.GetInfoBooleans", {"booleans": ["Player.PauseEnabled"]}).get("Player.PauseEnabled") is True
        except (ValueError, TypeError, AttributeError):
            return False

    def handle(self, method, params):
        if not self.managed:
            raise ValueError("Playback requires Isolde frontend mode")
        scheduled = method == "Isolde.PlayScheduled"
        offset = None
        if scheduled:
            if (set(params) != {"mediaType", "mediaId", "offsetSeconds"}
                or params.get("mediaType") not in {"movie", "episode", "musicvideo", "song"}
                or type(params.get("mediaId")) is not int or not 1 <= params["mediaId"] <= 2147483647
                or type(params.get("offsetSeconds")) is not int or not 0 <= params["offsetSeconds"] < 86400):
                raise ValueError("Scheduled playback requires a Kodi media identity and a bounded elapsed offset")
            media_type = params["mediaType"]
            offset = params["offsetSeconds"]
            method = {"movie":"Isolde.PlayMovie", "episode":"Isolde.PlayEpisode", "musicvideo":"Isolde.PlayMusicVideo", "song":"Isolde.PlaySong"}[media_type]
            params = {media_type+"id":params["mediaId"], "resume":False}
        if method in ("Isolde.PlayMovie", "Isolde.PlayEpisode", "Isolde.PlaySong", "Isolde.PlayChannel") or scheduled:
            media_type = "movie" if method == "Isolde.PlayMovie" else "musicvideo" if method == "Isolde.PlayMusicVideo" else "song" if method == "Isolde.PlaySong" else "channel" if method == "Isolde.PlayChannel" else "episode"
            key = media_type + "id"
            checked_pvr = media_type == "channel" and set(params) == {key,"resume","clientid","uniqueid"}
            if (not checked_pvr and set(params) != {key,"resume"}) or type(params.get(key)) is not int or not 1<=params[key]<=2147483647 or type(params.get("resume")) is not bool:
                raise ValueError("A library ID and explicit resume choice are required")
            if checked_pvr and any(type(params[field]) is not int or not -2147483648<=params[field]<=2147483647 for field in ("clientid","uniqueid")):
                raise ValueError("Invalid PVR source identity")
            if media_type in {"song","channel"} and params["resume"]:
                raise ValueError("Music and live TV do not use video resume")
            if self.movie is not None or self.rpc("Player.GetActivePlayers"):
                raise ValueError("Another playback session is active")
            if media_type == "channel":
                details = self.rpc("PVR.GetChannelDetails", {key:params[key],"properties":["channeltype","locked","hidden"]+(["clientid","uniqueid"] if checked_pvr else [])}).get("channeldetails",{})
                if details.get("channeltype") != "tv" or details.get("locked",False) or details.get("hidden",False):
                    raise ValueError("Choose an available unlocked Kodi TV channel")
                if checked_pvr and any(type(details.get(field)) is not int or details[field]!=params[field] for field in ("clientid","uniqueid")):
                    raise ValueError("PVR channel identity changed. Refresh and review its source mapping before tuning")
            else:
                duration_field = "duration" if media_type == "song" else "runtime"
                details = self.rpc("VideoLibrary.GetMovieDetails" if media_type == "movie" else "VideoLibrary.GetMusicVideoDetails" if media_type == "musicvideo" else "AudioLibrary.GetSongDetails" if media_type == "song" else "VideoLibrary.GetEpisodeDetails",
                                   {key: params[key], "properties": ["file", duration_field] if scheduled else ["file"]})
                path = details.get(media_type + "details", {}).get("file", "")
                if not isinstance(path, str) or not os.path.isabs(path) or not os.path.isfile(path) or not os.access(path, os.R_OK):
                    if scheduled:
                        # An explicit negative acknowledgement, not an RPC or
                        # player failure. Never expose the local path.
                        return {"state":"failed", "home":False, "availability":"unavailable",
                                "reason":"Scheduled local media is unavailable. Return to the guide to review a replacement."}
                    raise ValueError("This media is not an available local file")
                if scheduled:
                    duration = details.get(media_type+"details", {}).get(duration_field)
                    if type(duration) is not int or not 0 <= offset < duration <= 86400:
                        raise ValueError("Scheduled media duration changed or is unavailable; repair its slot before tuning")
            resume = {"hours":offset//3600, "minutes":offset//60%60, "seconds":offset%60, "milliseconds":0} if scheduled else params["resume"]
            if self.rpc("Player.Open", {"item": {key: params[key]}, "options": {"resume": resume}}) != "OK":
                raise ValueError("Kodi did not accept playback")
            self.movie, self.state, self.seen, self.home = params[key], "starting", False, False
            self.generation += 1
            self.media_type = media_type
            self.pending_stop = None
            self.scheduled_offset = offset
            self.opened_at = self.clock()
            self.failure_reason = ""
            self.control_error = ""
            self.can_pause = self.can_seek = False
            self.natural_end = self.stop_requested = False
            self.missing_since = None
            self.deadline = self.clock() + 20
            return self.status()
        if method == "Isolde.PlaybackStatus" and not params:
            return self.status()
        if method == "Isolde.PlaybackControl" and set(params) == {"action"}:
            return self.control(params["action"])
        raise ValueError("Unsupported playback request")

    def stopped_notification(self, data, generation):
        # Called only for Kodi's trusted Player.OnStop notification by service.
        # Missing/foreign identities never authorize another playback launch.
        if (type(generation) is not int or generation != self.generation
            or not self.managed or self.movie is None or not self.seen or self.stop_requested
            or not isinstance(data, dict) or data.get("end") is not True):
            return
        item = data.get("item")
        if (not isinstance(item, dict) or type(item.get("id")) is not int
            or item["id"] != self.movie or item.get("type") != self.media_type):
            return
        if self.rpc("Player.GetActivePlayers"):
            return
        self.natural_end = True

    def status(self):
        if self.movie is not None:
            players = self.rpc("Player.GetActivePlayers")
            videos = [p for p in players if p.get("type") == ("audio" if self.media_type == "song" else "video")]
            if videos:
                self.missing_since = None
                player = videos[0]["playerid"]
                item = self.rpc("Player.GetItem", {"playerid": player}).get("item", {})
                if item.get("type") != self.media_type or item.get("id") != self.movie:
                    if not self.seen and self.clock() < self.deadline:
                        return {"state": "starting", "home": self.home}
                    # Never stop a different player/item that we do not own.
                    self.movie, self.state = None, "replaced"
                else:
                    verify_seek = self.scheduled_offset is not None and self.scheduled_offset > 0 and not self.seen and self.pending_stop is None
                    props = self.rpc("Player.GetProperties", {"playerid": player, "properties": ["speed", "canseek", "time"] if verify_seek else ["speed", "canseek"]})
                    self.can_pause = self.pause_available()
                    self.can_seek = props.get("canseek") is True
                    if verify_seek:
                        position = props.get("time", {})
                        valid_position = (isinstance(position,dict)
                            and all(type(position.get(k)) is int for k in ("hours","minutes","seconds"))
                            and 0 <= position["hours"] <= 24 and 0 <= position["minutes"] < 60 and 0 <= position["seconds"] < 60)
                        seconds = position["hours"]*3600+position["minutes"]*60+position["seconds"] if valid_position else -1
                        elapsed = max(0,self.clock()-self.opened_at)
                        verified = (props.get("canseek") is True and valid_position
                            and self.scheduled_offset+elapsed-3 <= seconds <= self.scheduled_offset+elapsed+3)
                        if not verified:
                            # The owned player can appear before its demuxer
                            # reports seek capability or applies the open offset.
                            if self.clock() >= self.deadline:
                                # This is still the exact owned item checked above.
                                # Never report a restart-at-zero as a live tune.
                                self.rpc("Player.Stop", {"playerid":player})
                                self.movie, self.state = None, "failed"
                                self.failure_reason = "The player could not seek to this channel's broadcast position. Return to the guide and retry."
                                self.builtin("Minimize")
                                return {"state":self.state,"home":self.home,"reason":self.failure_reason}
                            return {"state":"starting","home":self.home}
                    if not self.seen:
                        self.builtin("ActivateWindow(visualisation)" if self.media_type == "song" else "ActivateWindow(fullscreenvideo)")
                    self.seen = True
                    self.state = "paused" if props.get("speed") == 0 else "playing"
                    if self.pending_stop is not None:
                        self.home = self.pending_stop == "home"
                        self.pending_stop = None
                        self.rpc("Player.Stop", {"playerid": player})
            elif self.seen or self.clock() >= self.deadline:
                # Give the callback pump one bounded interval to deliver Kodi's
                # identity-bearing end event. Unknown disappearance is a stop,
                # never implicit permission to continue a scheduled channel.
                if self.seen and self.scheduled_offset is not None and not self.stop_requested and not self.natural_end:
                    if self.missing_since is None: self.missing_since = self.clock()
                    if self.clock()-self.missing_since < 0.5:
                        return {"state":self.state,"home":self.home,"canPause":False,"canSeek":False}
                self.movie, self.state = None, "ended" if self.seen else "failed"
                self.builtin("Minimize")
        result = {"state": self.state, "home": self.home}
        result["canPause"] = self.movie is not None and self.can_pause
        result["canSeek"] = self.movie is not None and self.can_seek
        if self.state == "ended": result["naturalEnd"] = self.natural_end and not self.stop_requested
        if self.control_error: result["controlError"] = self.control_error
        if self.failure_reason: result["reason"] = self.failure_reason
        return result

    def control(self, action):
        if action not in ("pause", "stop", "home", "forward", "backward"):
            raise ValueError("Unsupported playback control")
        if action in ("stop", "home"):
            self.stop_requested = True
            self.natural_end = False
            self.home = action == "home"
        self.status()
        self.control_error = ""
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
                if not self.pause_available():
                    self.control_error = "This source does not support pausing. Playback continues."
                    return self.status()
                self.rpc("Player.PlayPause", {"playerid": pid, "play": "toggle"})
            else:
                props = self.rpc("Player.GetProperties", {"playerid": pid, "properties": ["canseek"]})
                if props.get("canseek") is not True:
                    self.control_error = "This source does not support seeking. Playback continues."
                    return self.status()
                self.rpc("Player.Seek", {"playerid": pid, "value": "smallforward" if action == "forward" else "smallbackward"})
        return self.status()
