"""Opt-in developer probe for an explicit isolated Kodi socket directory."""
import json
import os
import socket
import sys

directory, method = sys.argv[1:3]
if method not in {"JSONRPC.Version", "VideoLibrary.GetMovies", "VideoLibrary.Scan", "Isolde.PlaybackStatus"}:
    raise SystemExit("Read/scan probe only")
params = json.loads(sys.argv[3]) if len(sys.argv) == 4 else {}
fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
try:
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(5)
        client.connect(f"/proc/self/fd/{fd}/rpc.sock")
        client.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params}) + "\n").encode())
        reply = b""
        while not reply.endswith(b"\n"):
            chunk = client.recv(65536)
            if not chunk: raise RuntimeError("Incomplete reply")
            reply += chunk
            if len(reply) > 4 * 1024 * 1024: raise RuntimeError("Reply too large")
        print(reply.decode())
finally:
    os.close(fd)
