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
        self.options_request = 0

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
        if method == "Isolde.GetPlaybackOptions" and not params:
            return self.playback_options()
        if method == "Isolde.SetPlaybackOption" and set(params) == {"kind", "value"}:
            return self.set_playback_option(params["kind"], params["value"])
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
        if method == "Isolde.GetSubtitleTracks" and not params:
            return self.subtitle_tracks()
        if method == "Isolde.SetSubtitleTrack" and set(params) == {"subtitleId"}:
            subtitle_id = params["subtitleId"]
            if subtitle_id != "off" and (type(subtitle_id) is not int or subtitle_id < 0):
                raise ValueError("A valid subtitle track id or off is required")
            return self.set_subtitle_track(subtitle_id)
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
            self.options_request = 0
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
            self.options_request = 0
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
        if method == "Isolde.PlaybackControl" and set(params) == {"action", "position"} and params["action"] == "seek":
            return self.seek_to(params["position"])
        if method == "Isolde.PlaybackControl" and set(params) == {"action"}:
            return self.control(params["action"])
        if method == "Isolde.GetRecordings":
            return self.get_recordings(params)
        if method == "Isolde.DeleteRecording":
            return self.delete_recording(params)
        if method == "Isolde.GetTimers":
            return self.get_timers(params)
        if method == "Isolde.AddTimer":
            return self.add_timer(params)
        if method == "Isolde.DeleteTimer":
            return self.delete_timer(params)
        if method == "Isolde.GetChapters":
            return self.get_chapters(params)
        if method == "Isolde.SeekChapter":
            return self.seek_chapter(params)
        if method == "Isolde.GetMusicOptions":
            return self.get_music_options(params)
        if method == "Isolde.SetMusicOption":
            return self.set_music_option(params)
        if method == "Isolde.GetPVRGroups":
            return self.get_pvr_groups(params)
        if method == "Isolde.GetGroupMembers":
            return self.get_group_members(params)
        if method == "Isolde.AddToGroup":
            return self.add_to_group(params)
        if method == "Isolde.RemoveFromGroup":
            return self.remove_from_group(params)
        raise ValueError("Unsupported playback request")

    def owned_player(self):
        if not self.managed or self.movie is None:
            raise ValueError("An Isolde-managed player is required")
        for player in self.rpc("Player.GetActivePlayers"):
            ident=player.get("playerid")
            if player.get("type")==("audio" if self.media_type=="song" else "video") and type(ident) is int and self.owned_item(self.player_item(ident)):
                return ident
        raise ValueError("The managed player is no longer active")

    def owned_video_player(self):
        if self.media_type=="song":raise ValueError("An Isolde-managed video is required")
        return self.owned_player()

    def playback_options(self):
        """Read the current owned player's controls so the UI displays confirmed state."""
        player=self.owned_player()
        if self.media_type=="song":
            result = {"audio": [], "subtitles": [], "views": [], "view": "normal", "subtitleEnabled": False}
            props = self.rpc("Player.GetProperties", {"playerid": player, "properties": ["shuffled", "repeat"]})
            if type(props.get("shuffled")) is bool:
                result["shuffled"] = props["shuffled"]
            if props.get("repeat") in ("off", "one", "all"):
                result["repeat"] = props["repeat"]
            return result
        props=self.rpc("Player.GetProperties",{"playerid":player,"properties":["audiostreams","currentaudiostream","subtitles","currentsubtitle","subtitleenabled"]})
        result={"audio":[],"subtitles":[],"views":["normal","zoom","stretch4:3","widezoom","stretch16:9","original"],"subtitleEnabled":props.get("subtitleenabled") is True}
        for key,current in (("audiostreams","currentaudiostream"),("subtitles","currentsubtitle")):
            rows=props.get(key)
            if not isinstance(rows,list) or len(rows)>64:raise ValueError("Invalid track inventory")
            selected=props.get(current,{})
            if not isinstance(selected,dict):raise ValueError("Invalid current track")
            seen=set()
            for row in rows:
                if not isinstance(row,dict) or type(row.get("index")) is not int or row["index"]<0 or row["index"] in seen:raise ValueError("Invalid track identity")
                seen.add(row["index"])
                track={"id":row["index"],"name":str(row.get("name", ""))[:160],"language":str(row.get("language",""))[:32],"selected":row["index"]==selected.get("index") and (key=="audiostreams" or result["subtitleEnabled"])}
                if key=="audiostreams" and type(row.get("channels")) is int and 1 <= row["channels"] <= 32:
                    track["channels"]=row["channels"]
                if key=="subtitles" and row.get("isforced") is True:
                    track["forced"]=True
                result["audio" if key=="audiostreams" else "subtitles"].append(track)
        view=self.rpc("Player.GetViewMode")
        result["view"]=view.get("viewmode", "normal")
        return result

    def set_playback_option(self,kind,value):
        """Apply only a supported option to the currently owned player, then read it back."""
        if self.media_type == "song":
            available = self.playback_options()
            if kind == "shuffle" and type(value) is bool and "shuffled" in available:
                method = "Player.SetShuffle"
            elif kind == "repeat" and value in ("off", "one", "all") and "repeat" in available:
                method = "Player.SetRepeat"
            else:
                raise ValueError("Choose an available music playback option")
            if self.rpc(method, {"playerid": self.owned_player(), kind: value}) != "OK":
                raise ValueError("Kodi did not acknowledge the music playback option")
            return self.playback_options()
        before=self.playback_options();player=self.owned_video_player()
        if kind=="audio" and type(value) is int and any(row["id"]==value for row in before["audio"]):
            method="Player.SetAudioStream";params={"playerid":player,"stream":value}
        elif kind=="subtitle" and (value=="off" or type(value) is int and any(row["id"]==value for row in before["subtitles"])):
            method="Player.SetSubtitle";params={"playerid":player,"subtitle":value,"enable":value!="off"}
        elif kind=="view" and type(value) is str and value in before["views"]:
            method="Player.SetViewMode";params={"viewmode":value}
        elif kind=="audiooffset" and type(value) is int and -2000 <= value <= 2000:
            result = self.rpc("Player.SetAudioDelay", {"playerid": player, "offset": value / 1000})
            offset = result.get("offset") if isinstance(result, dict) else None
            if type(offset) not in (int, float) or not -2 <= offset <= 2:
                raise ValueError("Kodi did not confirm the audio offset")
            return {**self.playback_options(), "audioOffsetMs": round(offset * 1000)}
        elif kind=="speed" and type(value) in (int,float) and value in (1, 2, 4, 8):
            result = self.rpc("Player.SetSpeed", {"playerid": player, "speed": int(value)})
            speed = result.get("speed") if isinstance(result, dict) else None
            if type(speed) is not int or speed not in (1, 2, 4, 8):
                raise ValueError("Kodi did not confirm the playback speed")
            return {**self.playback_options(), "speed": speed}
        elif kind=="subtitleoffset" and type(value) is int and -2000 <= value <= 2000:
            method="Player.SetSubtitleDelay";params={"playerid":player,"delay":value}
        else:raise ValueError("Choose an available playback option")
        if self.rpc(method,params)!="OK":raise ValueError("Kodi did not acknowledge the option")
        return self.playback_options()

    def subtitle_tracks(self):
        if (self.movie is None or self.stream_url is not None or
                self.media_type not in ("movie", "episode", "musicvideo")):
            raise ValueError("Subtitle tracks require an Isolde-managed local video")
        for player in self.rpc("Player.GetActivePlayers"):
            if player.get("type") != "video":
                continue
            player_id = player.get("playerid")
            if type(player_id) is not int or not self.owned_item(self.player_item(player_id)):
                continue
            props = self.rpc("Player.GetProperties", {
                "playerid": player_id,
                "properties": ["subtitles", "currentsubtitle", "subtitleenabled"],
            })
            available = props.get("subtitles")
            if not isinstance(available, list) or len(available) > 64:
                raise ValueError("Kodi returned an invalid subtitle track list")
            selected = props.get("currentsubtitle", {})
            selected_id = selected.get("index") if isinstance(selected, dict) else None
            tracks = []
            seen = set()
            for entry in available:
                if not isinstance(entry, dict):
                    continue
                track_id = entry.get("index")
                if type(track_id) is not int or track_id < 0 or track_id in seen:
                    continue
                seen.add(track_id)
                tracks.append({
                    "id": track_id,
                    "name": str(entry.get("name", ""))[:160],
                    "language": str(entry.get("language", ""))[:32],
                    "external": entry.get("isexternal") is True,
                    "selected": track_id == selected_id and props.get("subtitleenabled") is True,
                })
            return {
                "tracks": tracks,
                "enabled": props.get("subtitleenabled") is True,
                "selectedId": selected_id if type(selected_id) is int else -1,
            }
        raise ValueError("The Isolde-managed video player is no longer active")

    def set_subtitle_track(self, subtitle_id):
        snapshot = self.subtitle_tracks()
        if subtitle_id != "off" and not any(track["id"] == subtitle_id for track in snapshot["tracks"]):
            raise ValueError("That subtitle track is not available on the managed video")
        for player in self.rpc("Player.GetActivePlayers"):
            if player.get("type") != "video":
                continue
            player_id = player.get("playerid")
            if type(player_id) is int and self.owned_item(self.player_item(player_id)):
                if self.rpc("Player.SetSubtitle", {
                    "playerid": player_id,
                    "subtitle": subtitle_id,
                    "enable": subtitle_id != "off",
                }) != "OK":
                    raise ValueError("Kodi did not confirm the subtitle track change")
                return self.subtitle_tracks()
        raise ValueError("The Isolde-managed video player is no longer active")

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

    @staticmethod
    def timeline_seconds(value):
        """Convert a bounded Kodi time object, leaving unknown timestamps unavailable."""
        limits = {"hours": 168, "minutes": 59, "seconds": 59}
        if not isinstance(value, dict) or any(type(value.get(k)) is not int or not 0 <= value[k] <= limit for k, limit in limits.items()):
            return None
        return value["hours"] * 3600 + value["minutes"] * 60 + value["seconds"]

    def seek_to(self, position):
        """Commit a previewed time only against the currently owned, seekable player."""
        if type(position) is not int or not 0 <= position <= 604800:
            raise ValueError("Invalid seek position")
        player = self.owned_player()
        props = self.rpc("Player.GetProperties", {"playerid": player, "properties": ["canseek", "totaltime"]})
        duration = self.timeline_seconds(props.get("totaltime"))
        if props.get("canseek") is not True or not duration or position >= duration:
            raise ValueError("This seek position is no longer available")
        if not self.owned_item(self.player_item(player)):
            raise ValueError("Playback ownership changed")
        hours, remainder = divmod(position, 3600)
        minutes, seconds = divmod(remainder, 60)
        self.rpc("Player.Seek", {"playerid": player, "value": {"time": {"hours": hours, "minutes": minutes, "seconds": seconds, "milliseconds": 0}}})
        self.control_error = ""
        return self.status()

    def status(self):
        timeline = {}
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
                    props = self.rpc("Player.GetProperties", {"playerid": player, "properties": ["speed", "canseek", "time", "totaltime"]})
                    position = self.timeline_seconds(props.get("time"))
                    duration = self.timeline_seconds(props.get("totaltime"))
                    if duration and position is not None:
                        timeline.update(position=min(position, duration), duration=duration)
                    label = item.get("label")
                    if isinstance(label, str):
                        timeline["title"] = label[:256]
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
        if self.options_request:result["optionsRequest"]=self.options_request
        if self.channel_delta:
            result.update(channelDelta=self.channel_delta, channelRequest=self.channel_request)
        result["canPause"] = self.movie is not None and self.can_pause
        result["canSeek"] = self.movie is not None and self.can_seek
        if self.movie is not None:
            result.update(timeline)
        if self.state == "ended": result["naturalEnd"] = self.natural_end and not self.stop_requested
        if self.control_error: result["controlError"] = self.control_error
        if self.failure_reason: result["reason"] = self.failure_reason
        return result

    def control(self, action):
        if action=="options":
            self.owned_player()
            self.options_request=self.options_request%2147483647+1
            return self.status()
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
                props = self.rpc("Player.GetProperties", {"playerid": pid, "properties": ["speed"]})
                self.rpc("Player.PlayPause", {"playerid": pid, "play": props["speed"] == 0})
            else:
                props = self.rpc("Player.GetProperties", {"playerid": pid, "properties": ["canseek"]})
                if props.get("canseek") is not True:
                    self.control_error = "This source does not support seeking. Playback continues."
                    return self.status()
                self.rpc("Player.Seek", {"playerid": pid, "value": {"step": "smallforward" if action == "forward" else "smallbackward"}})
        return self.status()

    def get_recordings(self, params):
        if set(params) != {}:
            raise ValueError("Recordings listing takes no parameters")
        result = self.rpc("PVR.GetRecordings", {})
        recordings = result.get("recordings", [])
        if not isinstance(recordings, list) or len(recordings) > 512:
            raise ValueError("Invalid recordings list from Kodi")
        items = []
        for rec in recordings:
            if not isinstance(rec, dict):
                continue
            rec_id = rec.get("recordedid")
            if type(rec_id) is not int or rec_id < 1:
                continue
            items.append({
                "id": rec_id,
                "title": str(rec.get("title", ""))[:160],
                "channel": str(rec.get("channelname", ""))[:80],
                "startTime": str(rec.get("starttime", ""))[:32],
                "endTime": str(rec.get("endtime", ""))[:32],
                "duration": rec.get("duration", 0),
                "plot": str(rec.get("plot", ""))[:512],
            })
        return {"recordings": items}

    def delete_recording(self, params):
        if set(params) != {"id"} or type(params.get("id")) is not int or params["id"] < 1:
            raise ValueError("A valid recording ID is required")
        result = self.rpc("PVR.DeleteRecorded", {"recordedid": params["id"]})
        if result != "OK":
            raise ValueError("Kodi did not confirm the recording deletion")
        return {"deleted": True}

    def get_timers(self, params):
        if set(params) != {}:
            raise ValueError("Timer listing takes no parameters")
        result = self.rpc("PVR.GetTimers", {})
        timers = result.get("timers", [])
        if not isinstance(timers, list) or len(timers) > 512:
            raise ValueError("Invalid timers list from Kodi")
        items = []
        for timer in timers:
            if not isinstance(timer, dict):
                continue
            timer_id = timer.get("timerid")
            if type(timer_id) is not int or timer_id < 1:
                continue
            items.append({
                "id": timer_id,
                "title": str(timer.get("title", ""))[:160],
                "channel": str(timer.get("channelname", ""))[:80],
                "startTime": str(timer.get("starttime", ""))[:32],
                "endTime": str(timer.get("endtime", ""))[:32],
                "type": str(timer.get("type", ""))[:40],
                "state": str(timer.get("state", ""))[:40],
                "plot": str(timer.get("plot", ""))[:512],
            })
        return {"timers": items}

    def add_timer(self, params):
        if set(params) != {"title", "channel", "startTime", "endTime"}:
            raise ValueError("Timer requires title, channel, startTime, and endTime")
        if type(params.get("channel")) is not int or params["channel"] < 1:
            raise ValueError("A valid channel ID is required")
        for field in ("startTime", "endTime"):
            if not isinstance(params.get(field), str) or not params[field]:
                raise ValueError(f"{field} must be a non-empty string")
        result = self.rpc("PVR.AddTimer", {
            "title": params["title"],
            "channelid": params["channel"],
            "starttime": params["startTime"],
            "endtime": params["endTime"],
        })
        if result != "OK":
            raise ValueError("Kodi did not confirm the timer creation")
        return {"added": True}

    def delete_timer(self, params):
        if set(params) != {"id"} or type(params.get("id")) is not int or params["id"] < 1:
            raise ValueError("A valid timer ID is required")
        result = self.rpc("PVR.DeleteTimer", {"timerid": params["id"]})
        if result != "OK":
            raise ValueError("Kodi did not confirm the timer deletion")
        return {"deleted": True}

    def get_chapters(self, params):
        if set(params) != {}:
            raise ValueError("Chapter listing takes no parameters")
        player = self.owned_player()
        props = self.rpc("Player.GetProperties", {
            "playerid": player,
            "properties": ["chapter", "chaptertime"],
        })
        chapters = props.get("chaptertime")
        if not isinstance(chapters, list) or len(chapters) > 256:
            raise ValueError("Invalid chapters list from Kodi")
        current = props.get("chapter")
        current_index = current.get("chapter") if isinstance(current, dict) else -1
        items = []
        for idx, ch in enumerate(chapters):
            if not isinstance(ch, dict):
                continue
            time_ms = ch.get("time")
            if type(time_ms) is not int or time_ms < 0:
                continue
            items.append({
                "index": idx,
                "time": time_ms,
                "name": str(ch.get("title", f"Chapter {idx + 1}"))[:160],
                "selected": idx == current_index,
            })
        return {"chapters": items, "current": current_index}

    def seek_chapter(self, params):
        if set(params) != {"index"} or type(params.get("index")) is not int or params["index"] < 0:
            raise ValueError("A valid chapter index is required")
        player = self.owned_player()
        props = self.rpc("Player.GetProperties", {
            "playerid": player,
            "properties": ["chaptertime"],
        })
        chapters = props.get("chaptertime")
        if not isinstance(chapters, list) or params["index"] >= len(chapters):
            raise ValueError("Chapter index out of range")
        chapter = chapters[params["index"]]
        time_ms = chapter.get("time")
        if type(time_ms) is not int or time_ms < 0:
            raise ValueError("Invalid chapter time")
        result = self.rpc("Player.Seek", {
            "playerid": player,
            "value": time_ms,
        })
        if result != "OK":
            raise ValueError("Kodi did not confirm the chapter seek")
        return {"seeked": True}

    def get_music_options(self, params):
        if set(params) != {}:
            raise ValueError("Music options listing takes no parameters")
        result = {}
        for key in ("music.crossfade", "music.gapless", "music.replaygain"):
            r = self.rpc("Settings.GetSettingValue", {"setting": key})
            result[key] = r.get("value")
        return {"options": result}

    def set_music_option(self, params):
        if set(params) != {"kind", "value"}:
            raise ValueError("Music option requires kind and value")
        kind = params["kind"]
        value = params["value"]
        if kind == "crossfade":
            if type(value) is not bool:
                raise ValueError("Crossfade must be a boolean")
            self.rpc("Settings.SetSettingValue", {"setting": "music.crossfade", "value": value})
        elif kind == "gapless":
            if type(value) is not bool:
                raise ValueError("Gapless must be a boolean")
            self.rpc("Settings.SetSettingValue", {"setting": "music.gapless", "value": value})
        elif kind == "replaygain":
            if value not in ("off", "track", "album", "auto"):
                raise ValueError("Replaygain must be one of off, track, album, auto")
            self.rpc("Settings.SetSettingValue", {"setting": "music.replaygain", "value": value})
        else:
            raise ValueError("Unknown music option kind")
        return {"set": True}

    def get_pvr_groups(self, params):
        """Return a bounded page of Kodi's television channel groups."""
        if params:
            raise ValueError("PVR groups listing takes no parameters")
        result = self.rpc("PVR.GetChannelGroups", {"channeltype": "tv", "limits": {"start": 0, "end": 50}})
        groups = result.get("channelgroups")
        if not isinstance(groups, list) or len(groups) > 50:
            raise ValueError("Invalid PVR groups list from Kodi")
        items = []
        for group in groups:
            if not isinstance(group, dict):
                continue
            group_id = group.get("channelgroupid")
            if type(group_id) is not int or group_id < 1:
                continue
            items.append({
                "id": group_id,
                "name": str(group.get("label", ""))[:160],
                "type": "tv",
            })
        return {"groups": items}

    def get_group_members(self, params):
        """Read one group's channels by Kodi group ID and keep the response scoped to that ID."""
        if set(params) != {"id"} or type(params.get("id")) is not int or params["id"] < 1:
            raise ValueError("A valid group ID is required")
        result = self.rpc("PVR.GetChannels", {"channelgroupid": params["id"], "limits": {"start": 0, "end": 50}})
        members = result.get("channels")
        if not isinstance(members, list) or len(members) > 50:
            raise ValueError("Invalid group members list from Kodi")
        items = []
        for member in members:
            if not isinstance(member, dict):
                continue
            member_id = member.get("channelid")
            if type(member_id) is not int or member_id < 1:
                continue
            items.append({
                "id": member_id,
                "title": str(member.get("label", ""))[:160],
                "type": "channel",
            })
        return {"members": items, "groupId": params["id"]}

    def add_to_group(self, params):
        if set(params) != {"id", "type"} or type(params.get("id")) is not int or params["id"] < 1:
            raise ValueError("A valid group ID and type are required")
        if params["type"] not in ("movie", "tvshow", "episode", "song", "musicvideo", "pvr"):
            raise ValueError("Invalid member type")
        result = self.rpc("PVR.AddToGroup", {
            "groupid": params["id"],
            "memberid": params.get("memberid", params["id"]),
            "membertype": params["type"],
        })
        if result != "OK":
            raise ValueError("Kodi did not confirm adding to group")
        return {"added": True}

    def remove_from_group(self, params):
        if set(params) != {"id", "type"} or type(params.get("id")) is not int or params["id"] < 1:
            raise ValueError("A valid group ID and type are required")
        if params["type"] not in ("movie", "tvshow", "episode", "song", "musicvideo", "pvr"):
            raise ValueError("Invalid member type")
        result = self.rpc("PVR.RemoveFromGroup", {
            "groupid": params["id"],
            "memberid": params.get("memberid", params["id"]),
            "membertype": params["type"],
        })
        if result != "OK":
            raise ValueError("Kodi did not confirm removing from group")
        return {"removed": True}
