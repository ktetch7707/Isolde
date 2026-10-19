import base64
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("artwork", Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge/artwork.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ArtworkTests(unittest.TestCase):
    def test_cached_art_uses_kodi_api_without_downloading(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "a").mkdir()
            image = b"\x89PNG\r\n\x1a\nfixture"
            (root / "a/test.png").write_bytes(image)
            calls = []
            def execute(raw):
                request = json.loads(raw); calls.append(request)
                result = {"episodedetails": {"art": {"thumb": "image://https%3A%2F%2Fexample.invalid%2Fart.png/"}}}
                if request["method"] == "Textures.GetTextures":
                    result = {"textures": [{"cachedurl": "a/test.png"}]}
                return json.dumps({"result": result})
            art = module.Artwork(execute, root)
            result = art.get({"kind": "episode", "id": 12})
            self.assertEqual(base64.b64decode(result["data"]), image)
            self.assertEqual([c["method"] for c in calls], ["VideoLibrary.GetEpisodeDetails", "Textures.GetTextures"])
            self.assertEqual(calls[1]["params"]["filter"]["value"], "https://example.invalid/art.png")
            self.assertEqual((root / "a/test.png").read_bytes(), image)

    def test_no_network_or_cache_population_for_missing_art(self):
        with tempfile.TemporaryDirectory() as directory:
            calls = []
            def execute(raw):
                request = json.loads(raw); calls.append(request["method"])
                result = {"tvshowdetails": {"art": {"poster": "https://example.invalid/missing.png"}}}
                if request["method"] == "Textures.GetTextures": result = {"textures": [{"cachedurl": "../private.png"}]}
                return json.dumps({"result": result})
            self.assertEqual(module.Artwork(execute, directory).get({"kind": "tvshow", "id": 1}), {"mime": "", "data": ""})
            self.assertEqual(calls, ["VideoLibrary.GetTVShowDetails", "Textures.GetTextures"])
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_rejects_paths_and_invalid_identities(self):
        art = module.Artwork(lambda _: self.fail("Invalid request reached Kodi"), "/tmp")
        for params in ({"file": "/etc/passwd"}, {"kind": "movie", "id": True}, {"kind": "movie", "id": 0},
                       {"kind": "unknown", "id": 1}, {"kind": "movie", "id": 1, "url": "https://example.invalid"}):
            with self.assertRaises(ValueError): art.get(params)


if __name__ == "__main__": unittest.main()
