#!/usr/bin/env python3
"""Developer probe: only an explicitly named isolated test Kodi home is modified.

Prepare creates a dedicated F8 RunScript keymap. It does not enable a network
service or alter Kodi databases. Production addon enablement is separate work.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket

parser = argparse.ArgumentParser()
parser.add_argument("action", choices=["prepare", "query"])
parser.add_argument("--home", type=Path, required=True)
args = parser.parse_args()
userdata = args.home / ".kodi/userdata"
if args.action == "prepare":
    if args.home.exists():
        parser.error("Preparation requires a new test home, never an existing user's Kodi home")
    addon = args.home / ".kodi/addons/service.isolde.bridge"
    shutil.copytree(Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge", addon)
    keymaps = userdata / "keymaps"
    keymaps.mkdir(parents=True)
    (keymaps / "isolde-probe.xml").write_text('<keymap><global><keyboard><f8>RunScript(special://home/addons/service.isolde.bridge/service.py)</f8></keyboard></global></keymap>')
    os.chmod(args.home, 0o700)
    print("Prepared isolated test home. Launch Kodi and press F8 once to start the read-only bridge.")
else:
    # A descriptor-relative path avoids Linux's 108-byte UNIX socket path limit
    # when the host-side profile directory has a UUID and long absolute prefix.
    descriptor = os.open(userdata / "isolde", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for number, (method, params) in enumerate([
            ("JSONRPC.Ping", {}), ("JSONRPC.Version", {}),
            ("Application.GetProperties", {"properties": ["name", "version"]}),
            ("VideoLibrary.GetMovies", {"properties": ["title", "year", "art", "resume", "playcount"], "limits": {"start": 0, "end": 10}}),
        ], 1):
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(5)
                client.connect(f"/proc/self/fd/{descriptor}/rpc.sock")
                client.sendall(json.dumps({"jsonrpc": "2.0", "id": number, "method": method, "params": params}).encode() + b"\n")
                response = b""
                while not response.endswith(b"\n"):
                    chunk = client.recv(65536)
                    if not chunk or len(response) + len(chunk) > 4194304:
                        raise RuntimeError("Invalid or oversized bridge response")
                    response += chunk
                result = json.loads(response)
                if result.get("id") != number or "error" in result:
                    raise RuntimeError(f"{method} failed")
                print(method, json.dumps(result["result"]))
    finally:
        os.close(descriptor)
