"""Opt-in developer probe for an explicit isolated Kodi socket directory."""

import json
import os
import socket
import sys


METHODS = {"JSONRPC.Version", "VideoLibrary.GetMovies", "VideoLibrary.Scan", "Isolde.PlaybackStatus"}
MAX_REPLY_BYTES = 4 * 1024 * 1024


def main():
    """Probe the chosen fixture socket with time and response bounds."""
    if len(sys.argv) not in (3, 4) or sys.argv[2] not in METHODS:
        raise SystemExit("Read/scan probe only")
    directory, method = sys.argv[1:3]
    params = json.loads(sys.argv[3]) if len(sys.argv) == 4 else {}
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(5)
            client.connect(f"/proc/self/fd/{fd}/rpc.sock")
            request = {"jsonrpc": "2.0", "id": 1, "method": method, "params": params}
            client.sendall((json.dumps(request) + "\n").encode())
            reply = bytearray()
            while not reply.endswith(b"\n"):
                chunk = client.recv(65536)
                if not chunk:
                    raise RuntimeError("Incomplete reply")
                reply.extend(chunk)
                if len(reply) > MAX_REPLY_BYTES:
                    raise RuntimeError("Reply too large")
        print(reply.decode())
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
