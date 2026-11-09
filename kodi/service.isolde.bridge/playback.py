"""Managed playback only: library IDs, local readable files, owned video player."""
import json
import os
import time
from urllib.parse import urlsplit, parse_qsl
from urllib.request import build_opener, ProxyHandler, HTTPRedirectHandler, Request
from urllib.error import HTTPError, URLError


class Playback:
    def __init__(self, execute, builtin, managed=False, clock=time.monotonic, subtitle_loader=None):
        self.execute, self.builtin, self.managed, self.clock = execute, builtin, managed, clock
        self.subtitle_loader = subtitle_loader
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
        self.stream_url = None
        self.channel_delta = 0
        self.channel_request = 0

    def owned_item(self, item):
        if self.stream_url is not None:
            return item.get("file") == self.stream_url
        return item.get("type") == self.media_type and item.get("id") == self.movie

    def player_item(self, player):
        params = {"playerid": player}
        if self.stream_url is not None:
            params["properties"] = ["file"]
        return self.rpc("Player.GetItem", params).get("item", {})

    def probe_stream(self, url):
        if urlsplit(url).scheme not in {"http", "https"}:
            return None
        class NoRedirect(HTTPRedirectHandler):
            def redirect_request(self, *args, **kwargs): return None
        try:
            # Only an explicit Tune probes the enabled source. No environment
            # proxies, response body, automatic refresh or redirect chain.
            with build_opener(ProxyHandler({}), NoRedirect()).open(Request(url, method="HEAD"), timeout=2): pass
        except HTTPError as error:
            error.close()
            if error.code in (401,403):
                return {"sourceState":"authentication-required", "reason":"This live source requires authentication or entitlement. Open its supported provider app or choose an accessible stream. Cached listings are unchanged."}
            if error.code in (404,410):
                return {"sourceState":"unavailable", "reason":"The live source no longer provides this stream. Review its URL and retry; cached listings are unchanged."}
            if error.code in (301,302,303,307,308,405,501): return None
            return {"sourceState":"connection-failed", "reason":"The live source reported a connection or server failure. Retry later; cached listings are unchanged."}
        except (URLError, OSError, TimeoutError):
            return {"sourceState":"connection-failed", "reason":"Cannot connect to this live source. Check the connection and retry; cached listings are unchanged."}
        return None

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
        if method == "Isolde.LoadSubtitle":
            path = params.get("path")
            if (set(params) != {"path"} or not isinstance(path, str) or
                not os.path.isabs(path) or not path.lower().endswith(".srt") or
                not os.path.isfile(path) or os.path.getsize(path) > 8 * 1024 * 1024):
                raise ValueError("A readable local SRT is required")
            if self.movie is None or self.media_type not in ("movie", "episode", "musicvideo") or self.stream_url is not None:
                raise ValueError("An Isolde-managed local video must be playing")
            if not self.owned_item(self.player_item(1)):
                raise ValueError("The active Kodi video is not the Isolde-managed media item")
            if self.subtitle_loader is None:
                raise ValueError("Kodi subtitle loading is unavailable")
            self.subtitle_loader(path)
            return {"loaded": True}
        scheduled = method == "Isolde.PlayScheduled"
        offset = None
        if method == "Isolde.PlayGuideStream":
            url = params.get("url")
            if set(params) != {"url"} or not isinstance(url, str) or not 1 <= len(url) <= 4096:
                raise ValueError("A reviewed compatible stream URL is required")
            parsed = urlsplit(url)
            if (parsed.scheme not in {"http", "https", "rtsp", "rtp", "udp"} or not parsed.hostname
                or parsed.username not in (None, "") or parsed.password is not None or parsed.fragment
                or (parsed.port is not None and not 1 <= parsed.port <= 65535)
                or "|" in url or any(c.isspace() or ord(c) < 32 or ord(c) == 127 for c in url)):
                raise ValueError("Stream credentials, player options and unsupported protocols are refused")
            for key, _ in parse_qsl(parsed.query, keep_blank_values=True, max_num_fields=256):
                key = key.casefold().replace("-", "").replace("_", "")
                if key in {"key","auth","user","username","pass","sig","jwt","ticket"} or any(part in key for part in ("password","secret","token","signature","credential","authorization","apikey","session","bearer")):
                    raise ValueError("Credential-bearing stream URLs require a configured PVR backend or provider app")
            if self.movie is not None or self.rpc("Player.GetActivePlayers"):
                raise ValueError("Another playback session is active")
            failure = self.probe_stream(url)
            if failure:
                self.state, self.home, self.failure_reason = "failed", False, failure["reason"]
                return {"state":"failed", "home":False, **failure}
            if self.rpc("Player.Open", {"item": {"file": url}}) != "OK":
                raise ValueError("Kodi did not accept the live stream")
            self.stream_url, self.movie, self.media_type = url, 1, "stream"
            self.channel_delta = 0
            self.state, self.seen, self.home = "starting", False, False
            self.generation += 1
            self.pending_stop = self.scheduled_offset = self.missing_since = None
            self.failure_reason = self.control_error = ""
            self.can_pause = self.can_seek = self.natural_end = self.stop_requested = False
            self.opened_at = self.clock()
            self.deadline = self.clock() + 20
            return self.status()
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
            self.stream_url = None
            self.channel_delta = 0
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
                item = self.player_item(player)
                if not self.owned_item(item):
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
                if not self.seen and self.stream_url is not None:
                    self.failure_reason = "The live stream could not start. Check its connection, sign-in requirements and supported format, then retry. Cached listings are unchanged."
                self.movie, self.state = None, "ended" if self.seen else "failed"
                self.builtin("Minimize")
        result = {"state": self.state, "home": self.home}
        if self.channel_delta:
            result.update(channelDelta=self.channel_delta, channelRequest=self.channel_request)
        result["canPause"] = self.movie is not None and self.can_pause
        result["canSeek"] = self.movie is not None and self.can_seek
        if self.state == "ended": result["naturalEnd"] = self.natural_end and not self.stop_requested
        if self.control_error: result["controlError"] = self.control_error
        if self.failure_reason: result["reason"] = self.failure_reason
        return result

    def control(self, action):
        if action in ("channel-next", "channel-previous"):
            self.status()
            if self.movie is not None and self.seen:
                self.channel_delta = 1 if action == "channel-next" else -1
                self.channel_request = self.channel_request % 2147483647 + 1
            return self.status()
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
            item = self.player_item(pid)
            if not self.owned_item(item):
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
