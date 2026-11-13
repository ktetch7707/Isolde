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
    def test_music_art_uses_audio_library_identity_without_network(self):
        with tempfile.TemporaryDirectory() as directory:
            calls = []
            def execute(raw):
                request = json.loads(raw); calls.append(request)
                kind = "album" if request["method"] == "AudioLibrary.GetAlbumDetails" else "song"
                return json.dumps({"result": {kind+"details": {"art": {}}}})
            art = module.Artwork(execute, directory)
            for kind in ("album", "song"):
                self.assertEqual(art.get({"kind":kind,"id":41}), {"mime":"","data":""})
            self.assertEqual([call["method"] for call in calls], ["AudioLibrary.GetAlbumDetails","AudioLibrary.GetSongDetails"])
            self.assertEqual(calls[0]["params"], {"albumid":41,"properties":["art"]})

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

    def test_fresh_art_uses_bounded_kodi_image_vfs_and_closes_it(self):
        class Image:
            def __init__(self, size):self.length=size;self.closed=False;self.reads=[]
            def size(self):return self.length
            def readBytes(self,limit):self.reads.append(limit);return b'\x89PNG\r\n\x1a\nfixture'
            def close(self):self.closed=True
        with tempfile.TemporaryDirectory() as directory:
            url='https://example.invalid/fresh.png';opened=[]
            def execute(raw):
                method=json.loads(raw)['method']
                return json.dumps({'result':{'textures':[]} if method=='Textures.GetTextures' else {'moviedetails':{'art':{'poster':url,'fanart':url}}}})
            for size in (32,1024*1024+1,0):
                image=Image(size)
                def open_image(path):opened.append(path);return image
                result=module.Artwork(execute,directory,open_image).get({'kind':'movie','id':1})
                self.assertTrue(image.closed)
                self.assertEqual(bool(result['data']),size==32)
                self.assertEqual(image.reads,[1024*1024+1] if size==32 else [])
            self.assertEqual(opened,['image://https%3A%2F%2Fexample.invalid%2Ffresh.png/']*3)
            self.assertEqual(list(Path(directory).iterdir()),[])


if __name__ == "__main__": unittest.main()
