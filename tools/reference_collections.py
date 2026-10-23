"""Disposable, offline provider fixtures for the ordinary Home executable.

Only acceptance tooling imports this module. No application switches, renderer
branches, user profiles, Kodi databases or executable launchers are changed.
"""
import base64
import io
import json
import os
from pathlib import Path
import socket
import threading


class ReferenceCollections:
    def __init__(self, evidence, fixture, repository):
        self.requests = []
        self.errors = []
        self.installed = []
        self.fixture = fixture
        self.evidence = evidence
        self.stopped = threading.Event()
        self.art = {}
        # These are actual packaged catalog definitions restored through the
        # normal installed-state boundary. They are never launched by this test.
        for relative in json.loads((repository / "config/catalog.json").read_text())["packages"]:
            manifest = repository / "config" / relative
            data = json.loads(manifest.read_text())
            if data["app"]["id"] not in fixture["installedApps"]:
                continue
            destination = evidence / "data/Isolde/packages" / data["packageId"] / data["version"] / "manifest.json"
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(manifest.read_bytes())
            self.installed.append(data["app"]["id"])
        if set(self.installed) != set(fixture["installedApps"]):
            raise ValueError("Fixture references an unregistered app")
        (evidence / "config/Isolde/installed-apps.json").write_text(json.dumps({
            "installed": self.installed,
            "packageStates": {app: "ready" for app in self.installed},
        }))
        # Generate original geometric test artwork; no reference screenshot or
        # illustrated card is used as a rendered Home surface.
        from PIL import Image, ImageDraw
        for index, item in enumerate(fixture["history"]):
            image = Image.new("RGB", (640, 360))
            draw = ImageDraw.Draw(image)
            a, b = fixture["palettes"][index % len(fixture["palettes"])]
            for y in range(360):
                t = y / 359
                draw.line((0, y, 640, y), fill=tuple(round(a[k]*(1-t)+b[k]*t) for k in range(3)))
            draw.ellipse((445, 30, 530, 115), fill=(253, 239, 218))
            draw.polygon([(0, 300), (150, 130), (290, 280), (420, 165), (640, 310), (640, 360), (0, 360)], fill=tuple(b))
            output = io.BytesIO()
            image.save(output, format="PNG")
            self.art[(item["kind"], item["id"])] = base64.b64encode(output.getvalue()).decode()
        directory = evidence / "data/Isolde/native-profiles/default/kodi/home/.kodi/userdata/isolde"
        directory.mkdir(parents=True, mode=0o700)
        self.socket = socket.socket(socket.AF_UNIX)
        descriptor = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            self.socket.bind(f"/proc/self/fd/{descriptor}/rpc.sock")
        finally:
            os.close(descriptor)
        self.socket.listen(16)
        self.socket.settimeout(0.2)
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def serve(self):
        while not self.stopped.is_set():
            try:
                connection, _ = self.socket.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            with connection:
                connection.settimeout(2)
                try:
                    message = b""
                    while not message.endswith(b"\n") and len(message) <= 65536:
                        chunk = connection.recv(65536)
                        if not chunk:
                            raise ValueError("Incomplete fixture request")
                        message += chunk
                    request = json.loads(message)
                    self.requests.append(request)
                    result = self.reply(request)
                    connection.sendall(json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": result}).encode()+b"\n")
                except (OSError, ValueError, KeyError) as error:
                    self.errors.append(str(error))

    def reply(self, request):
        method, params = request["method"], request.get("params", {})
        if method == "JSONRPC.Version":
            return {"version": {"major": 13}}
        if method == "Isolde.GetArtwork":
            data = self.art.get((params["kind"], params["id"]), "")
            return {"mime": "image/png" if data else "", "data": data}
        if method not in {"VideoLibrary.GetMovies", "VideoLibrary.GetEpisodes"}:
            raise ValueError("Unsupported fixture request: " + method)
        kind = "movie" if method.endswith("Movies") else "episode"
        items = [{kind+"id": item["id"], "title": item["title"], "label": item["title"],
                  "lastplayed": item["viewedAt"], "year": 2026, "showtitle": item.get("show", ""),
                  "playcount": item["playcount"], "resume": item["resume"]}
                 for item in self.fixture["history"] if item["kind"] == kind]
        items.sort(key=lambda item: item["lastplayed"], reverse=True)
        start, end = params.get("limits", {}).get("start", 0), params.get("limits", {}).get("end", 50)
        page = items[start:end]
        return {"movies" if kind == "movie" else "episodes": page,
                "limits": {"start": start, "end": start+len(page), "total": len(items)}}

    def verify_history(self):
        for method in ("VideoLibrary.GetMovies", "VideoLibrary.GetEpisodes"):
            matches = [r for r in self.requests if r["method"] == method and
                       r.get("params", {}).get("sort", {}).get("method") == "lastplayed"]
            if not matches:
                raise AssertionError("Production history renderer did not request " + method)
            for request in matches:
                params = request["params"]
                assert params["limits"] == {"start": 0, "end": 50}
                assert "lastplayed" in params["properties"]
        assert self.art and any(r["method"] == "Isolde.GetArtwork" for r in self.requests)
        assert not self.errors, self.errors

    def close(self):
        self.stopped.set()
        self.socket.close()
        self.thread.join(timeout=3)
        (self.evidence / "provider-requests.json").write_text(json.dumps({
            "requests": self.requests, "errors": self.errors,
            "installedCatalogApps": self.installed,
            "fixtureOnly": True, "realPlaybackVerified": False,
        }, indent=2))
