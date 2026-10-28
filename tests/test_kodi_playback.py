import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import runpy
import sys
import types
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("playback", Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/playback.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PlaybackTests(unittest.TestCase):
    def test_service_pumps_callbacks_and_accepts_normalized_remote_sender(self):
        events = []
        monitors = []
        class Monitor:
            def __init__(self): monitors.append(self)
            def abortRequested(self): return False
            def waitForAbort(self, timeout):
                events.append("pump")
                return events.count("pump") == 2
        class Player:
            managed = True
            movie = None
            def __init__(self, *args): pass
            def control(self, action): events.append(action)
        class Bridge:
            def __init__(self, *args): pass
            def start(self): pass
            def poll(self):
                if "pump" not in events: monitors[0].onNotification("isolde", "Other.pause", "")
            def close(self): events.append("closed")
        fake = types.SimpleNamespace(Monitor=Monitor, executeJSONRPC=lambda _: None,
            executebuiltin=lambda _: None, log=lambda *args: None, LOGINFO=1, LOGWARNING=2, LOGERROR=3)
        modules = {"xbmc": fake, "xbmcvfs": types.SimpleNamespace(translatePath=lambda p: p),
                   "bridge": types.SimpleNamespace(LocalBridge=Bridge), "playback": types.SimpleNamespace(Playback=Player),
                   "artwork": types.SimpleNamespace(Artwork=lambda *args: None),
                   "playlists": types.SimpleNamespace(Playlists=lambda *args: None)}
        with patch.dict(sys.modules, modules):
            runpy.run_path(str(Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/service.py"))
        self.assertEqual(events, ["pump", "pause", "pump", "closed"])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "movie.ogv"
        self.path.write_bytes(b"fixture")
        self.active = False
        self.item = 7
        self.kind = "movie"
        self.channel_locked = False
        self.omit_pvr_uniqueid = False
        self.speed = 1
        self.duration = 7200
        self.position = 0
        self.seekable = True
        self.pausable = True
        self.pause_info = None
        self.pause_info_error = False
        self.now = 0
        self.calls, self.builtins = [], []
        self.player = module.Playback(self.execute, self.builtins.append, True, lambda: self.now)

    def execute(self, raw):
        request = json.loads(raw)
        method, params = request["method"], request["params"]
        self.calls.append((method, params))
        result = "OK"
        if method == "Player.GetActivePlayers": result = [{"playerid": 1, "type": "audio" if self.kind == "song" else "video"}] if self.active else []
        elif method == "VideoLibrary.GetMovieDetails": result = {"moviedetails": {"file": str(self.path),"runtime":self.duration}}
        elif method == "VideoLibrary.GetEpisodeDetails": result = {"episodedetails": {"file": str(self.path),"runtime":self.duration}}
        elif method == "VideoLibrary.GetMusicVideoDetails": result = {"musicvideodetails": {"file": str(self.path),"runtime":self.duration}}
        elif method == "AudioLibrary.GetSongDetails": result = {"songdetails": {"file": str(self.path),"duration":self.duration}}
        elif method == "PVR.GetChannelDetails":
            details={"channeltype":"tv","hidden":False,"locked":self.channel_locked}
            for key,value in (("clientid",9),("uniqueid",101)):
                if key in params["properties"] and not (key=="uniqueid" and self.omit_pvr_uniqueid):details[key]=value
            result={"channeldetails":details}
        elif method == "Player.GetItem": result = {"item": {"type": self.kind, "id": self.item}}
        elif method == "Player.GetProperties":
            values={"speed":self.speed,"canseek":self.seekable,"canchangespeed":True,"live":self.kind=="channel",
                    "time":{"hours":self.position//3600,"minutes":self.position//60%60,"seconds":self.position%60}}
            if set(params["properties"])-set(values):return json.dumps({"error":{"code":-32602,"message":"Invalid Player property"}})
            result={key:values[key] for key in params["properties"]}
        elif method == "XBMC.GetInfoBooleans":
            self.assertEqual(params,{"booleans":["Player.PauseEnabled"]})
            if self.pause_info_error:return json.dumps({"error":{"code":-32602,"message":"Capability unavailable"}})
            result={"Player.PauseEnabled":self.pausable} if self.pause_info is None else self.pause_info
        elif method == "Player.PlayPause": self.speed = 0 if self.speed else 1
        elif method == "Player.Stop": self.active = False
        return json.dumps({"result": result})

    def test_live_tv_uses_configured_channel_identity_and_refuses_locks(self):
        self.kind="channel";self.channel_locked=True
        with self.assertRaises(ValueError): self.player.handle("Isolde.PlayChannel",{"channelid":7,"resume":False})
        self.assertFalse(any(method=="Player.Open" for method,_ in self.calls))
        self.channel_locked=False
        self.assertEqual(self.player.handle("Isolde.PlayChannel",{"channelid":7,"resume":False})["state"],"starting")
        self.active=True;self.assertEqual(self.player.status()["state"],"playing")
        self.assertIn(("Player.Open",{"item":{"channelid":7},"options":{"resume":False}}),self.calls)
        self.assertEqual(self.player.control("home"),{"state":"ended","home":True,"canPause":False,"canSeek":False,"naturalEnd":False})

    def test_unsupported_live_controls_keep_owned_playback_running(self):
        self.kind = "channel"
        self.player.handle("Isolde.PlayChannel", {"channelid": 7, "resume": False})
        self.active = True
        self.seekable = self.pausable = False
        status = self.player.status()
        self.assertFalse(status["canPause"])
        self.assertFalse(status["canSeek"])
        self.calls.clear()
        for action in ("pause", "forward", "backward"):
            result = self.player.control(action)
            self.assertEqual(result["state"], "playing")
            self.assertTrue(result["controlError"])
            self.assertTrue(self.active)
        self.assertFalse(any(method in ("Player.PlayPause", "Player.Seek", "Player.Stop") for method, _ in self.calls))

    def test_unknown_pause_capability_preserves_playing_and_refuses_pause(self):
        for info,error in (({"Player.PauseEnabled":False},False),({},False),
                           ({"Player.PauseEnabled":"true"},False),({"Player.PauseEnabled":1},False),
                           (False,False),({},True)):
            with self.subTest(info=info,error=error):
                self.pause_info,self.pause_info_error=info,error
                self.player=module.Playback(self.execute,self.builtins.append,True,lambda:self.now)
                self.active=False;self.calls=[]
                self.player.handle("Isolde.PlayMovie",{"movieid":7,"resume":False});self.active=True
                state=self.player.status();self.assertEqual(state["state"],"playing");self.assertIs(state["canPause"],False)
                result=self.player.control("pause");self.assertEqual(result["state"],"playing");self.assertTrue(result["controlError"])
                self.assertFalse(any(method in {"Player.PlayPause","Player.Stop"} for method,_ in self.calls))
                self.assertTrue(self.active)

    def test_pvr_import_tune_revalidates_backend_identity(self):
        self.kind="channel"
        params={"channelid":7,"resume":False,"clientid":9,"uniqueid":102}
        with self.assertRaises(ValueError):self.player.handle("Isolde.PlayChannel",params)
        self.assertFalse(any(method=="Player.Open" for method,_ in self.calls))
        params["uniqueid"]=101
        self.assertEqual(self.player.handle("Isolde.PlayChannel",params)["state"],"starting")
        self.assertIn(("PVR.GetChannelDetails",{"channelid":7,"properties":["channeltype","locked","hidden","clientid","uniqueid"]}),self.calls)
        self.assertIn(("Player.Open",{"item":{"channelid":7},"options":{"resume":False}}),self.calls)
        self.active=True;self.assertEqual(self.player.status()["state"],"playing")
        self.assertEqual(self.player.control("home"),{"state":"ended","home":True,"canPause":False,"canSeek":False,"naturalEnd":False})

    def test_checked_pvr_tune_rejects_missing_requested_identity_without_open(self):
        self.omit_pvr_uniqueid=True
        with self.assertRaises(ValueError):
            self.player.handle("Isolde.PlayChannel",{"channelid":7,"resume":False,"clientid":9,"uniqueid":101})
        self.assertFalse(any(method=="Player.Open" for method,_ in self.calls))

    def test_scheduled_tune_uses_absolute_broadcast_time_and_verifies_seek(self):
        self.kind="episode"
        result=self.player.handle("Isolde.PlayScheduled",{"mediaType":"episode","mediaId":7,"offsetSeconds":4511})
        self.assertEqual(result["state"],"starting")
        self.assertIn(("Player.Open",{"item":{"episodeid":7},"options":{"resume":{"hours":1,"minutes":15,"seconds":11,"milliseconds":0}}}),self.calls)
        self.active=True
        self.assertEqual(self.player.status()["state"],"starting") # player has not applied its seek yet
        self.position=4511
        self.assertEqual(self.player.status()["state"],"playing")
        self.assertIn("ActivateWindow(fullscreenvideo)",self.builtins)

    def test_scheduled_seek_failure_is_not_reported_as_live_playback(self):
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"movie","mediaId":7,"offsetSeconds":800})
        self.active=True;self.seekable=False
        for instant in (0,19.99):
            self.now=instant;self.assertEqual(self.player.status()["state"],"starting");self.assertTrue(self.active)
        self.assertFalse(any(method=="Player.Stop" for method,_ in self.calls))
        self.now=20
        result=self.player.status()
        self.assertEqual(result["state"],"failed");self.assertIn("broadcast position",result["reason"])
        self.assertFalse(self.active)
        self.assertNotIn("ActivateWindow(fullscreenvideo)",self.builtins)

    def test_scheduled_startup_waits_for_seek_capability_and_position(self):
        self.kind="episode";self.seekable=False
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"episode","mediaId":7,"offsetSeconds":800});self.active=True
        self.assertEqual(self.player.status()["state"],"starting");self.now=1
        self.assertEqual(self.player.status()["state"],"starting");self.assertTrue(self.active)
        self.assertNotIn("ActivateWindow(fullscreenvideo)",self.builtins)
        self.seekable=True;self.assertEqual(self.player.status()["state"],"starting") # Capability alone is not successful seek.
        self.position=801;self.assertEqual(self.player.status()["state"],"playing")
        self.assertIn("ActivateWindow(fullscreenvideo)",self.builtins)
        self.assertFalse(any(method=="Player.Stop" for method,_ in self.calls))

    def test_scheduled_position_must_follow_live_clock_not_eventually_reach_initial_offset(self):
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"movie","mediaId":7,"offsetSeconds":5});self.active=True
        self.now=5;self.position=5;self.assertEqual(self.player.status()["state"],"starting")
        self.now=6;self.position=11;self.assertEqual(self.player.status()["state"],"playing")

    def test_restart_at_zero_never_catches_up_to_a_moving_broadcast_position(self):
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"movie","mediaId":7,"offsetSeconds":5});self.active=True
        for instant in (0,1,5,10,19):
            self.now=instant;self.position=instant;self.assertEqual(self.player.status()["state"],"starting");self.assertTrue(self.active)
        self.assertNotIn("ActivateWindow(fullscreenvideo)",self.builtins)
        self.now=self.position=20;self.assertEqual(self.player.status()["state"],"failed");self.assertFalse(self.active)

    def test_scheduled_startup_home_and_replacement_preserve_ownership(self):
        self.seekable=False
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"movie","mediaId":7,"offsetSeconds":800})
        self.player.control("home");self.active=True;self.player.status()
        self.assertFalse(self.active);self.assertTrue(self.player.status()["home"])
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"movie","mediaId":7,"offsetSeconds":800})
        self.active=True;self.item=9;self.now=21
        self.calls=[]
        self.assertEqual(self.player.status()["state"],"replaced")
        self.assertNotIn("Player.Stop",[m for m,_ in self.calls])

    def test_scheduled_offset_and_duration_validation_precedes_player_open(self):
        valid={"mediaType":"movie","mediaId":7,"offsetSeconds":800}
        for invalid in (dict(valid,mediaType="channel"),dict(valid,mediaId=True),dict(valid,mediaId=0),
                        dict(valid,offsetSeconds=-1),dict(valid,offsetSeconds=True),dict(valid,offsetSeconds=86400),
                        dict(valid,file=str(self.path)),dict(valid,offsetSeconds="800")):
            with self.assertRaises(ValueError):self.player.handle("Isolde.PlayScheduled",invalid)
        self.duration=700
        with self.assertRaises(ValueError):self.player.handle("Isolde.PlayScheduled",valid)
        self.assertNotIn("Player.Open",[m for m,_ in self.calls])

    def test_deleted_scheduled_local_media_returns_typed_unavailable_without_open(self):
        for kind in ("movie","episode","musicvideo","song"):
            with self.subTest(kind=kind):
                self.kind=kind;self.path.write_bytes(b"fixture")
                self.player=module.Playback(self.execute,self.builtins.append,True,lambda:self.now)
                params={"mediaType":kind,"mediaId":7,"offsetSeconds":0}
                self.assertEqual(self.player.handle("Isolde.PlayScheduled",params)["state"],"starting")
                self.assertIn("Player.Open",[method for method,_ in self.calls])
                self.path.unlink();self.calls=[]
                self.player=module.Playback(self.execute,self.builtins.append,True,lambda:self.now)
                result=self.player.handle("Isolde.PlayScheduled",params)
                self.assertEqual(result["state"],"failed")
                self.assertIs(result["home"],False)
                self.assertEqual(result["availability"],"unavailable")
                self.assertNotIn(str(self.path),json.dumps(result))
                self.assertNotIn("Player.Open",[method for method,_ in self.calls])
                details=("AudioLibrary.GetSongDetails" if kind=="song" else "VideoLibrary.Get"+{"movie":"Movie","episode":"Episode","musicvideo":"MusicVideo"}[kind]+"Details")
                self.assertIn((details,{kind+"id":7,"properties":["file","duration" if kind=="song" else "runtime"]}),self.calls)
        with self.assertRaises(ValueError):
            self.player.handle("Isolde.PlayMovie",{"movieid":7,"resume":False})

    def test_scheduled_musicvideo_uses_kodi_library_identity(self):
        self.kind="musicvideo"
        self.player.handle("Isolde.PlayScheduled",{"mediaType":"musicvideo","mediaId":7,"offsetSeconds":0})
        self.active=True
        self.assertEqual(self.player.status()["state"],"playing")
        self.assertIn(("VideoLibrary.GetMusicVideoDetails",{"musicvideoid":7,"properties":["file","runtime"]}),self.calls)

    def test_local_song_ownership_and_audio_controls(self):
        self.kind = "song"
        with self.assertRaises(ValueError): self.player.handle("Isolde.PlaySong", {"songid":7,"resume":True})
        self.assertEqual(self.player.handle("Isolde.PlaySong", {"songid":7,"resume":False})["state"], "starting")
        self.active = True
        self.assertEqual(self.player.status()["state"], "playing")
        self.assertIn("ActivateWindow(visualisation)", self.builtins)
        self.assertNotIn("ActivateWindow(fullscreenvideo)", self.builtins)
        self.assertEqual(self.player.control("pause")["state"], "paused")
        self.item = 99
        self.assertEqual(self.player.status()["state"], "replaced")
        self.assertFalse(any(method == "Player.Stop" for method, _ in self.calls))

    def start(self):
        self.assertEqual(self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": True})["state"], "starting")
        self.active = True
        self.assertEqual(self.player.status()["state"], "playing")

    def test_resume_pause_seek_home(self):
        self.start()
        self.assertIn(("Player.Open", {"item": {"movieid": 7}, "options": {"resume": True}}), self.calls)
        self.assertEqual(self.player.control("pause")["state"], "paused")
        self.player.control("forward")
        self.assertIn(("Player.Seek", {"playerid": 1, "value": "smallforward"}), self.calls)
        result = self.player.control("home")
        self.assertEqual(result, {"state": "ended", "home": True, "canPause": False, "canSeek": False, "naturalEnd": False})
        self.assertEqual(self.builtins, ["ActivateWindow(fullscreenvideo)", "Minimize"])
        self.assertEqual(self.path.read_bytes(), b"fixture")

    def test_episode_resume_restart_and_type_scoped_ownership(self):
        self.kind = "episode"
        self.player.handle("Isolde.PlayEpisode", {"episodeid": 7, "resume": False})
        self.assertIn(("Player.Open", {"item": {"episodeid": 7}, "options": {"resume": False}}), self.calls)
        self.active = True
        self.assertEqual(self.player.status()["state"], "playing")
        self.kind = "movie"  # same numeric ID is a different item
        self.assertEqual(self.player.control("home")["state"], "replaced")
        self.assertNotIn("Player.Stop", [m for m, p in self.calls])
        self.assertEqual(self.path.read_bytes(), b"fixture")

    def test_episode_requires_local_library_id(self):
        for params in ({"episodeid": True, "resume": True}, {"file": str(self.path)}, {"episodeid": 0, "resume": True}):
            with self.assertRaises(ValueError): self.player.handle("Isolde.PlayEpisode", params)
        self.path.unlink()
        with self.assertRaises(ValueError): self.player.handle("Isolde.PlayEpisode", {"episodeid": 7, "resume": True})
        self.assertNotIn("Player.Open", [m for m, p in self.calls])

    def test_natural_end_and_start_timeout(self):
        self.start()
        self.active = False
        self.assertEqual(self.player.status()["state"], "ended")
        self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": False})
        self.now = 21
        self.assertEqual(self.player.status()["state"], "failed")

    def stopped(self, event):
        self.player.stopped_notification(event, self.player.generation)

    def test_scheduled_completion_requires_matching_natural_end_event(self):
        self.player.handle("Isolde.PlayScheduled", {"mediaType":"movie", "mediaId":7, "offsetSeconds":0})
        self.active=True;self.assertEqual(self.player.status()["state"],"playing")
        self.stopped({"end":True,"item":{"id":7,"type":"movie"}})
        self.assertFalse(self.player.natural_end) # active/replacement player is never an end authorization
        self.active=False
        self.player.stopped_notification({"end":True,"item":{"id":7,"type":"movie"}}, self.player.generation-1)
        self.assertFalse(self.player.natural_end)
        for event in ({"end":False,"item":{"id":7,"type":"movie"}},
                      {"end":True,"item":{"id":8,"type":"movie"}},
                      {"end":True,"item":{"id":7,"type":"episode"}}, {"end":True}):
            self.stopped(event)
            self.assertFalse(self.player.natural_end)
        self.assertEqual(self.player.status()["state"],"playing") # bounded callback grace
        self.stopped({"end":True,"item":{"id":7,"type":"movie"}})
        result=self.player.status();self.assertEqual(result["state"],"ended");self.assertTrue(result["naturalEnd"])

    def test_scheduled_missing_callback_and_explicit_stop_never_continue(self):
        self.player.handle("Isolde.PlayScheduled", {"mediaType":"movie", "mediaId":7, "offsetSeconds":0})
        self.active=True;self.player.status();self.active=False
        self.assertEqual(self.player.status()["state"],"playing")
        self.now=0.6
        result=self.player.status();self.assertEqual(result["state"],"ended");self.assertFalse(result["naturalEnd"])
        self.player.handle("Isolde.PlayScheduled", {"mediaType":"movie", "mediaId":7, "offsetSeconds":0})
        self.active=True;self.player.status();self.player.control("stop")
        self.stopped({"end":True,"item":{"id":7,"type":"movie"}})
        self.assertFalse(self.player.status()["naturalEnd"])

    def test_home_during_startup_stops_when_player_appears(self):
        self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": True})
        self.player.control("home")
        self.assertEqual(self.player.pending_stop, "home")
        self.active = True
        self.player.status()
        self.assertFalse(self.active)
        self.assertEqual(self.player.status(), {"state": "ended", "home": True, "canPause": False, "canSeek": False, "naturalEnd": False})

    def test_transient_unknown_item_during_open_is_not_replacement(self):
        self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": True})
        self.active = True
        self.item = -1
        self.assertEqual(self.player.status()["state"], "starting")
        self.item = 7
        self.assertEqual(self.player.status()["state"], "playing")

    def test_replacement_is_not_stopped(self):
        self.start()
        self.item = 8
        self.assertEqual(self.player.control("stop")["state"], "replaced")
        self.assertNotIn("Player.Stop", [m for m, p in self.calls])

    def test_rejects_untrusted_and_unavailable_requests(self):
        for params in ({"movieid": True, "resume": True}, {"movieid": 7, "resume": "true"},
                       {"file": str(self.path)}, {"movieid": -1, "resume": True}):
            with self.assertRaises(ValueError): self.player.handle("Isolde.PlayMovie", params)
        self.path.unlink()
        with self.assertRaises(ValueError): self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": True})
        self.assertNotIn("Player.Open", [m for m, p in self.calls])
        self.player.managed = False
        with self.assertRaises(ValueError): self.player.handle("Isolde.PlaybackStatus", {})


if __name__ == "__main__": unittest.main()
