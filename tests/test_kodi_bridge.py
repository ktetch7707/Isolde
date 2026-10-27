import importlib.util
import json
import os
from pathlib import Path
import socket
import tempfile
import threading
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location("bridge", Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/bridge.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class BridgeTests(unittest.TestCase):
    def test_pvr_broadcast_reads_are_bounded_and_read_only(self):
        calls=[]
        def execute(raw):
            calls.append(json.loads(raw));return json.dumps({"result":{"broadcasts":[],"limits":{"start":0,"end":0,"total":0}}})
        params={"channelid":7,"properties":["title","starttime","endtime"],"limits":{"start":0,"end":50}}
        self.assertIn("result",bridge.dispatch(self.request("PVR.GetBroadcasts",params),execute))
        self.assertEqual(calls[0]["method"],"PVR.GetBroadcasts")
        for invalid in (dict(params,channelid=True),dict(params,channelid=0),dict(params,limits={"start":0,"end":51}),dict(params,properties=["file"]),dict(params,exec="anything")):
            self.assertIn("error",bridge.dispatch(self.request("PVR.GetBroadcasts",invalid),lambda _:self.fail("Invalid PVR read reached Kodi")))

    def test_guide_candidates_join_show_metadata_without_scanning_or_mutating(self):
        calls=[]
        def execute(raw):
            request=json.loads(raw);calls.append(request);method=request["method"];params=request["params"]
            if method=="VideoLibrary.GetEpisodes":
                self.assertEqual(params["limits"],{"start":0,"end":50})
                result={"episodes":[{"episodeid":7,"tvshowid":9,"title":"Local cartoon","runtime":1200,"file":"/media/cartoon.mkv","showtitle":"Series","season":1,"episode":1,"firstaired":"2020-01-02"},
                    {"episodeid":8,"tvshowid":9,"title":"Remote","runtime":1200,"file":"https://example.invalid/video"}],"limits":{"start":0,"end":2,"total":2}}
            elif method=="VideoLibrary.GetTVShowDetails":
                self.assertEqual(params["tvshowid"],9);result={"tvshowdetails":{"studio":["Cartoon Network"],"genre":["Animation"],"tag":["Family"],"year":2019}}
            else:self.fail("Unexpected/mutating guide method: "+method)
            return json.dumps({"result":result})
        result=bridge.dispatch(self.request("Isolde.GuideCandidates",{"kind":"episode","start":0,"end":50}),execute)["result"]
        self.assertEqual(result["skipped"],1);self.assertEqual(len(result["items"]),1)
        item=result["items"][0];self.assertEqual(item["mediaType"],"episode");self.assertEqual(item["mediaId"],7)
        self.assertEqual(item["studio"],["Cartoon Network"]);self.assertEqual(item["genre"],["Animation"]);self.assertEqual(item["year"],2020)
        self.assertEqual([r["method"] for r in calls],["VideoLibrary.GetEpisodes","VideoLibrary.GetTVShowDetails"])
        for invalid in ({"kind":"shell","start":0,"end":50},{"kind":"movie","start":False,"end":50},{"kind":"movie","start":0,"end":51},{"kind":"movie","start":0,"end":50,"file":"/private"}):
            result=bridge.dispatch(self.request("Isolde.GuideCandidates",invalid),lambda _:self.fail("Invalid parameters reached Kodi"))
            self.assertIn("error",result)

    @unittest.skipUnless(os.name=="posix","Kodi playlist writes run inside the Linux guest")
    def test_create_playlist_is_kodi_owned_atomic_and_profile_bounded(self):
        module_spec=importlib.util.spec_from_file_location("isolde_playlists",Path(__file__).resolve().parents[1]/"kodi/service.isolde.bridge/playlists.py")
        module=importlib.util.module_from_spec(module_spec);module_spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            profile=Path(directory)/"profile";profile.mkdir()
            store=module.Playlists(profile,True)
            params={"name":"Family & friends","kind":"movies","view":"genre","value":"Adventure"}
            result=store.create(params);folder=profile/"playlists/video";saved=folder/result["playlistName"]
            self.assertTrue(saved.is_file());self.assertEqual(saved.stat().st_mode & 0o777,0o600)
            xml=ET.fromstring(saved.read_bytes());self.assertEqual(xml.attrib["type"],"movies")
            self.assertEqual(xml.findtext("name"),"Family & friends");self.assertEqual(xml.find("rule").attrib,{"field":"genre","operator":"is"})
            self.assertEqual(xml.findtext("rule/value"),"Adventure")
            before={p.name:p.read_bytes() for p in folder.iterdir()}
            real_fsync=module.os.fsync;count=0
            def fail_publication(descriptor):
                nonlocal count
                count+=1
                if count==2:raise OSError("Injected directory fsync failure")
                return real_fsync(descriptor)
            with patch.object(module.os,"fsync",side_effect=fail_publication):
                with self.assertRaises(OSError):store.create(dict(params,name="Should roll back"))
            self.assertEqual({p.name:p.read_bytes() for p in folder.iterdir()},before)
            for invalid in (dict(params,name=""),dict(params,view="shell"),dict(params,value="x\x00y"),dict(params,directory="/private")):
                with self.assertRaises(ValueError):store.create(invalid)
            with self.assertRaises(ValueError):module.Playlists(profile,False).create(params)
            other=Path(directory)/"other";other.mkdir();linked=Path(directory)/"linked";linked.symlink_to(other,target_is_directory=True)
            with self.assertRaises(OSError):module.Playlists(linked,True).create(params)
            self.assertEqual(list(other.iterdir()),[])
            reply=bridge.dispatch(self.request("Isolde.CreatePlaylist",params),lambda _:self.fail("No generic RPC for playlist write"),playlists=store)
            self.assertTrue(reply["result"]["saved"])
            self.assertIn("error",bridge.dispatch(self.request("Isolde.CreatePlaylist",params),lambda _:None))

    def test_saved_playlist_catalog_and_members_are_read_only_and_bounded(self):
        calls=[]
        def execute(raw):
            req=json.loads(raw);calls.append(req)
            self.assertEqual(req["method"],"Files.GetDirectory")
            params=req["params"];start=params["limits"]["start"]
            self.assertLessEqual(params["limits"]["end"]-start,50)
            if params["directory"].endswith("/video/"):
                files=[{"file":"special://profile/playlists/video/Family films.xsp","label":"Family films"},
                       {"file":"/private/secret.xsp","label":"Foreign"}]
            else:
                self.assertEqual(params["directory"],"special://profile/playlists/video/Family films.xsp")
                files=[{"id":41,"type":"movie","title":"Local film","file":"/private/media.mkv","resume":{"position":7}}]
            return json.dumps({"result":{"files":files,"limits":{"start":start,"end":start+len(files),"total":start+len(files)}}})
        catalog=bridge.dispatch(self.request("Isolde.GetPlaylists"),execute)["result"]
        self.assertEqual([p["playlistName"] for p in catalog["playlists"]],["Family films.xsp"])
        result=bridge.dispatch(self.request("Isolde.GetPlaylistItems",{"name":"Family films.xsp","kind":"movies","start":0,"end":50}),execute)["result"]
        self.assertEqual(result["movies"][0]["movieid"],41)
        self.assertNotIn("file",result["movies"][0])
        self.assertEqual(result["movies"][0]["resume"]["position"],7)
        count=len(calls)
        for name in ("../secret.xsp","/secret.xsp","https://private.xsp","%2e%2e.xsp","bad\x00.xsp","x.txt"):
            self.assertIn("error",bridge.dispatch(self.request("Isolde.GetPlaylistItems",{"name":name,"kind":"movies","start":0,"end":50}),execute))
        self.assertIn("error",bridge.dispatch(self.request("Isolde.GetPlaylistItems",{"name":"Family films.xsp","kind":"movies","start":0,"end":51}),execute))
        self.assertIn("error",bridge.dispatch(self.request("Isolde.GetPlaylists",{"directory":"/private"}),execute))
        self.assertEqual(len(calls),count)

    def test_guide_reads_only_bounded_configured_tv_channels(self):
        calls=[]
        def execute(raw):
            calls.append(json.loads(raw));return '{"jsonrpc":"2.0","id":7,"result":{}}'
        valid={"channelgroupid":"alltv","properties":["channelnumber","broadcastnow","broadcastnext"],"limits":{"start":0,"end":50}}
        self.assertNotIn("error",bridge.dispatch(self.request("PVR.GetChannels",valid),execute))
        for change in ({"channelgroupid":"allradio"},{"limits":{"start":0,"end":51}},{"properties":["file"]},{"url":"https://example.invalid"}):
            self.assertIn("error",bridge.dispatch(self.request("PVR.GetChannels",dict(valid,**change)),execute))
        self.assertEqual(len(calls),1)

    def test_music_reads_are_bounded_and_reject_arbitrary_filters(self):
        calls = []
        def execute(raw):
            calls.append(json.loads(raw))
            return '{"jsonrpc":"2.0","id":7,"result":{}}'
        for method, params in (("AudioLibrary.GetAlbums", {"properties":["title","artist","art"],"limits":{"start":0,"end":50}}),
                               ("AudioLibrary.GetSongs", {"properties":["title","track"],"filter":{"albumid":41},"limits":{"start":50,"end":100}})):
            self.assertNotIn("error", bridge.dispatch(self.request(method, params), execute))
        for params in ({"filter":{"file":"/private/path"}}, {"filter":{"albumid":True}}, {"filter":{"albumid":0}},
                       {"limits":{"start":0,"end":51}}, {"properties":["file"]}, {"sort":{"method":"random"}}, {"directory":"/private"}):
            self.assertIn("error", bridge.dispatch(self.request("AudioLibrary.GetSongs", params), execute))
        self.assertEqual(len(calls),2)

    def test_local_search_boundary(self):
        calls = []
        def execute(raw):
            calls.append(json.loads(raw))
            return '{"jsonrpc":"2.0","id":7,"result":{"movies":[]}}'
        for field, operator, value in (("title", "contains", "Movie"), ("tag", "is", "Family"),
                                       ("genre", "is", "Drama"), ("inprogress", "true", ""), ("lastplayed", "after", "1970-01-01")):
            result = bridge.dispatch(self.request("VideoLibrary.GetMovies", {"filter": {"field": field, "operator": operator, "value": value}}), execute)
            self.assertNotIn("error", result)
        for rule in ({"field": "playlist", "operator": "is", "value": "https://example.com"},
                     {"url": "https://example.com"}, {"field": "title", "operator": "contains", "value": "x" * 161},
                     {"field":"lastplayed","operator":"after","value":"2026-99-99"},
                     {"field":"lastplayed","operator":"after","value":"20261021"}):
            self.assertIn("error", bridge.dispatch(self.request("VideoLibrary.GetMovies", {"filter": rule}), execute))
        self.assertEqual(len(calls), 5)
        self.assertTrue(all(c["method"] == "VideoLibrary.GetMovies" for c in calls))

    def test_quit_checks_playback_and_both_scanners(self):
        for active, video, music, allowed in (([], False, False, True),
                ([{"playerid": 1}], False, False, False), ([], True, False, False),
                ([], False, True, False), ([], None, False, False)):
            methods = []
            def execute(raw):
                request = json.loads(raw)
                methods.append(request["method"])
                results = {"Player.GetActivePlayers": active,
                           "XBMC.GetInfoBooleans": {"Library.IsScanningVideo": video, "Library.IsScanningMusic": music},
                           "Application.Quit": "OK"}
                return json.dumps({"jsonrpc": "2.0", "id": 7, "result": results[request["method"]]})
            result = bridge.dispatch(self.request("Isolde.QuitIfIdle"), execute)
            self.assertEqual("Application.Quit" in methods, allowed)
            self.assertEqual("error" not in result, allowed)
        self.assertIn("error", bridge.dispatch(self.request("Application.Quit"), lambda _: self.fail("Raw quit allowed")))
        self.assertIn("error", bridge.dispatch(self.request("Isolde.QuitIfIdle", {"force": True}), lambda _: self.fail("Forced quit allowed")))

    def request(self, method, params=None):
        return json.dumps({"jsonrpc": "2.0", "id": 7, "method": method, "params": params or {}})

    def test_supported_read_and_bounded_paging(self):
        received = []
        def execute(raw):
            received.append(json.loads(raw))
            return '{"jsonrpc":"2.0","id":7,"result":{"movies":[]}}'
        result = bridge.dispatch(self.request("VideoLibrary.GetMovies"), execute)
        self.assertEqual(result["result"]["movies"], [])
        self.assertEqual(received[0]["params"]["limits"]["end"], 50)
        result = bridge.dispatch(self.request("VideoLibrary.GetMovies", {"limits": {"end": 10000}}), execute)
        self.assertIn("error", result)
        self.assertEqual(len(received), 1)

    def test_movie_detail_is_read_only_and_bounded(self):
        received = []
        def execute(raw):
            received.append(json.loads(raw))
            return json.dumps({"jsonrpc": "2.0", "id": 7, "result": {
                "moviedetails": {"movieid": 12, "title": "Fixture movie", "year": 2024}}})
        valid = {"movieid": 12, "properties": ["title", "year"]}
        self.assertEqual(bridge.dispatch(self.request("VideoLibrary.GetMovieDetails", valid), execute)["result"]["moviedetails"]["movieid"], 12)
        self.assertEqual(received[0]["params"], valid)
        for params in (
            {"movieid": 0, "properties": ["title"]},
            {"movieid": True, "properties": ["title"]},
            {"movieid": 12.5, "properties": ["title"]},
            {"movieid": 12, "properties": ["file"]},
            {"movieid": 12, "properties": ["title"], "path": "/etc/passwd"},
        ):
            self.assertIn("error", bridge.dispatch(self.request("VideoLibrary.GetMovieDetails", params), execute))
        self.assertEqual(len(received), 1)

    def test_tvshow_detail_is_read_only_and_bounded(self):
        received = []
        def execute(raw):
            received.append(json.loads(raw))
            return json.dumps({"jsonrpc": "2.0", "id": 7, "result": {
                "tvshowdetails": {"tvshowid": 4, "title": "Fixture show", "episode": 8}}})
        valid = {"tvshowid": 4, "properties": ["title", "episode"]}
        result = bridge.dispatch(self.request("VideoLibrary.GetTVShowDetails", valid), execute)
        self.assertEqual(result["result"]["tvshowdetails"]["tvshowid"], 4)
        self.assertEqual(received[0]["params"], valid)
        for params in (
            {"tvshowid": 0, "properties": ["title"]},
            {"tvshowid": 4, "properties": ["file"]},
            {"tvshowid": 4, "properties": ["title"], "path": "/etc/passwd"},
        ):
            self.assertIn("error", bridge.dispatch(self.request("VideoLibrary.GetTVShowDetails", params), execute))
        self.assertEqual(len(received), 1)

    def test_rejects_commands_and_bad_requests(self):
        def forbidden(_):
            self.fail("Unsupported request reached Kodi")
        for raw in ("[1]", "{", self.request("System.Shutdown"), self.request("Files.GetDirectory")):
            self.assertIn("error", bridge.dispatch(raw, forbidden))

    def test_scan_is_restricted_to_configured_sources(self):
        received = []
        def execute(raw):
            received.append(json.loads(raw))
            return '{"jsonrpc":"2.0","id":7,"result":"OK"}'
        self.assertEqual(bridge.dispatch(self.request("VideoLibrary.Scan"), execute)["result"], "OK")
        self.assertEqual(received[0]["params"], {"showdialogs": False})
        for method, params in (
            ("VideoLibrary.Scan", {"directory": "/etc"}),
            ("VideoLibrary.Scan", {"showdialogs": True}),
            ("VideoLibrary.Clean", {}),
            ("XBMC.GetInfoBooleans", {"booleans": ["System.HasNetwork"]}),
        ):
            self.assertIn("error", bridge.dispatch(self.request(method, params), execute))
        self.assertEqual(len(received), 1)
        bridge.dispatch(self.request("XBMC.GetInfoBooleans", {"booleans": ["Library.IsScanningVideo"]}), execute)
        self.assertEqual(len(received), 2)

    def test_local_socket_permissions_and_duplicate(self):
        with tempfile.TemporaryDirectory() as directory:
            server = bridge.LocalBridge(Path(directory) / "private", lambda raw: '{"jsonrpc":"2.0","id":7,"result":"pong"}')
            server.start()
            self.addCleanup(server.close)
            self.assertEqual(server.socket_path.stat().st_mode & 0o777, 0o600)
            with self.assertRaises(RuntimeError):
                bridge.LocalBridge(server.directory, lambda raw: raw).start()
            # Duplicate detection creates an empty connection; drain it first.
            server.poll()
            worker = threading.Thread(target=server.poll)
            worker.start()
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(2)
                client.connect(str(server.socket_path))
                client.sendall(self.request("JSONRPC.Ping").encode() + b"\n")
                self.assertEqual(json.loads(client.recv(4096))["result"], "pong")
            worker.join(timeout=2)
            self.assertFalse(worker.is_alive())
            server.close()
            self.assertFalse(server.socket_path.exists())

    def test_existing_file_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "rpc.sock"
            path.write_text("user data")
            with self.assertRaises(RuntimeError):
                bridge.LocalBridge(directory, lambda raw: raw).start()
            self.assertEqual(path.read_text(), "user data")

    def test_stale_socket_recovered(self):
        with tempfile.TemporaryDirectory() as directory:
            with socket.socket(socket.AF_UNIX) as stale:
                stale.bind(str(Path(directory) / "rpc.sock"))
            server = bridge.LocalBridge(directory, lambda raw: raw)
            server.start()
            server.close()


if __name__ == "__main__":
    unittest.main()
