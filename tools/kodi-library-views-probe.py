"""Read-only acceptance against an explicit test-profile Kodi socket directory."""
import json
import os
import socket
import sys

def query(directory, field=None, operator=None, value=""):
    params = {"properties": ["title", "resume"], "limits": {"start": 0, "end": 50}}
    if field:
        params["filter"] = {"field": field, "operator": operator, "value": value}
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(5)
            client.connect(f"/proc/self/fd/{fd}/rpc.sock")
            client.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": "VideoLibrary.GetMovies", "params": params}) + "\n").encode())
            raw = b""
            while not raw.endswith(b"\n"):
                part = client.recv(65536)
                if not part: raise RuntimeError("Incomplete reply")
                raw += part
                if len(raw) > 4 * 1024 * 1024: raise RuntimeError("Reply too large")
            reply = json.loads(raw)
            if "error" in reply: raise RuntimeError(reply["error"])
            return reply["result"]
    finally:
        os.close(fd)

if __name__ == "__main__":
    directory = sys.argv[1]
    all_movies = query(directory)
    match = query(directory, "title", "contains", "Isolde Playback Fixture")
    assert any(m["title"] == "Isolde Playback Fixture" for m in match.get("movies", [])), "Test fixture missing"
    missing = query(directory, "title", "contains", "no-such-isolde-fixture-641902")
    assert missing["limits"]["total"] == 0
    progress = query(directory, "inprogress", "true")
    assert all(m.get("resume", {}).get("position", 0) > 0 for m in progress.get("movies", []))
    assert any(m["title"] == "Isolde Curated Fixture" for m in progress.get("movies", [])), "Resumable fixture missing"
    for field, name in (("tag", "Isolde Test Collection"), ("genre", "Isolde Test Genre")):
        found = query(directory, field, "is", name)
        assert any(m["title"] == "Isolde Curated Fixture" for m in found.get("movies", [])), field + " fixture missing"
    for field in ("tag", "genre"):
        assert query(directory, field, "is", "no-such-isolde-fixture-641902")["limits"]["total"] == 0
    print(json.dumps({"library_total": all_movies["limits"]["total"], "title_matches": match["limits"]["total"],
                      "continue_watching": progress["limits"]["total"], "missing_title_tag_genre": "passed"}))
