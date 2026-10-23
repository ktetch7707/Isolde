import importlib.util
import json
import os
from pathlib import Path
import socket
import tempfile
import threading
import unittest

spec = importlib.util.spec_from_file_location("bridge", Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/bridge.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class BridgeTests(unittest.TestCase):
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
