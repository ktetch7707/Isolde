#!/usr/bin/env python3
"""Prepare or query the Kodi bridge in an explicitly selected test home."""

import argparse
import json
import os
from pathlib import Path
import shutil
import socket


def prepare(home):
    """Create the bridge and keymap in a fresh fixture home."""
    home.mkdir(mode=0o700)
    addon = home / ".kodi/addons/service.isolde.bridge"
    source = Path(__file__).resolve().parents[1] / "kodi/service.isolde.bridge"
    shutil.copytree(source, addon)
    keymaps = home / ".kodi/userdata/keymaps"
    keymaps.mkdir(parents=True)
    (keymaps / "isolde-probe.xml").write_text(
        "<keymap><global><keyboard><f8>"
        "RunScript(special://home/addons/service.isolde.bridge/service.py)"
        "</f8></keyboard></global></keymap>",
        encoding="utf-8",
    )
    print("Prepared isolated test home. Launch Kodi and press F8 once to start the read-only bridge.")


def query(userdata):
    """Use a short descriptor-relative socket path and bound all fixture replies."""
    requests = (
        ("JSONRPC.Ping", {}),
        ("JSONRPC.Version", {}),
        ("Application.GetProperties", {"properties": ["name", "version"]}),
        ("VideoLibrary.GetMovies", {
            "properties": ["title", "year", "art", "resume", "playcount"],
            "limits": {"start": 0, "end": 10},
        }),
    )
    descriptor = os.open(userdata / "isolde", os.O_RDONLY | os.O_DIRECTORY)
    try:
        for number, (method, params) in enumerate(requests, 1):
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(5)
                client.connect(f"/proc/self/fd/{descriptor}/rpc.sock")
                request = {"jsonrpc": "2.0", "id": number, "method": method, "params": params}
                client.sendall(json.dumps(request).encode() + b"\n")
                response = bytearray()
                while not response.endswith(b"\n"):
                    chunk = client.recv(65536)
                    if not chunk or len(response) + len(chunk) > 4 * 1024 * 1024:
                        raise RuntimeError("Invalid or oversized bridge response")
                    response.extend(chunk)
            result = json.loads(response)
            if result.get("id") != number or "error" in result:
                raise RuntimeError(f"{method} failed")
            print(method, json.dumps(result["result"]))
    finally:
        os.close(descriptor)


def main():
    """Require a named fixture home for both preparation and queries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "query"))
    parser.add_argument("--home", type=Path, required=True)
    args = parser.parse_args()
    if args.action == "prepare":
        if args.home.exists() or args.home.is_symlink():
            parser.error("Preparation requires a new test home, never an existing user's Kodi home")
        prepare(args.home)
    else:
        query(args.home / ".kodi/userdata")


if __name__ == "__main__":
    main()
