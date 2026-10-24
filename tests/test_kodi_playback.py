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
                return True
        class Player:
            managed = True
            movie = None
            def __init__(self, *args): pass
            def control(self, action): events.append(action)
        class Bridge:
            def __init__(self, *args): pass
            def start(self): pass
            def poll(self): monitors[0].onNotification("isolde", "Other.pause", "")
            def close(self): events.append("closed")
        fake = types.SimpleNamespace(Monitor=Monitor, executeJSONRPC=lambda _: None,
            executebuiltin=lambda _: None, log=lambda *args: None, LOGINFO=1, LOGWARNING=2, LOGERROR=3)
        modules = {"xbmc": fake, "xbmcvfs": types.SimpleNamespace(translatePath=lambda p: p),
                   "bridge": types.SimpleNamespace(LocalBridge=Bridge), "playback": types.SimpleNamespace(Playback=Player),
                   "artwork": types.SimpleNamespace(Artwork=lambda *args: None)}
        with patch.dict(sys.modules, modules):
            runpy.run_path(str(Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/service.py"))
        self.assertEqual(events, ["pause", "pump", "closed"])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "movie.ogv"
        self.path.write_bytes(b"fixture")
        self.active = False
        self.item = 7
        self.kind = "movie"
        self.channel_locked = False
        self.speed = 1
        self.now = 0
        self.calls, self.builtins = [], []
        self.player = module.Playback(self.execute, self.builtins.append, True, lambda: self.now)

    def execute(self, raw):
        request = json.loads(raw)
        method, params = request["method"], request["params"]
        self.calls.append((method, params))
        result = "OK"
        if method == "Player.GetActivePlayers": result = [{"playerid": 1, "type": "audio" if self.kind == "song" else "video"}] if self.active else []
        elif method == "VideoLibrary.GetMovieDetails": result = {"moviedetails": {"file": str(self.path)}}
        elif method == "VideoLibrary.GetEpisodeDetails": result = {"episodedetails": {"file": str(self.path)}}
        elif method == "AudioLibrary.GetSongDetails": result = {"songdetails": {"file": str(self.path)}}
        elif method == "PVR.GetChannelDetails": result = {"channeldetails":{"channeltype":"tv","hidden":False,"locked":self.channel_locked}}
        elif method == "Player.GetItem": result = {"item": {"type": self.kind, "id": self.item}}
        elif method == "Player.GetProperties": result = {"speed": self.speed}
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
        self.assertEqual(self.player.control("home"),{"state":"ended","home":True})

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
        self.assertEqual(result, {"state": "ended", "home": True})
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

    def test_home_during_startup_stops_when_player_appears(self):
        self.player.handle("Isolde.PlayMovie", {"movieid": 7, "resume": True})
        self.player.control("home")
        self.assertEqual(self.player.pending_stop, "home")
        self.active = True
        self.player.status()
        self.assertFalse(self.active)
        self.assertEqual(self.player.status(), {"state": "ended", "home": True})

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
