#!/usr/bin/env python3
"""Read-only, offline inspection. Does not start Kodi or open capture devices."""
import json
from pathlib import Path
import shutil
import subprocess

CAPTURE_ELEMENTS = ("v4l2src", "videoconvert", "videoscale", "decodebin",
                    "audioconvert", "audioresample", "pulsesrc", "pulsesink", "appsink")


def probe(arguments):
    try:
        result = subprocess.run(arguments, capture_output=True, text=True, timeout=2, check=False)
        return result.returncode == 0, result.stdout.strip()[:512]
    except (OSError, subprocess.TimeoutExpired):
        return False, ""


def inspect(find=shutil.which, run=probe, video_root=Path("/sys/class/video4linux")):
    kodi = find("kodi")
    valid, version = run(["dpkg-query", "-W", "-f=${Version}", "kodi"]) if kodi else (False, "")
    rows = [{"name": "Kodi", "version": version if valid else "Unknown",
             "status": "Installed — session health is shown in Media Library" if kodi else "Missing — install required backend packages"}]
    gst = find("gst-inspect-1.0")
    valid, version = run([gst, "--version"]) if gst else (False, "")
    missing = [element for element in CAPTURE_ELEMENTS if not run([gst, element])[0]] if valid else list(CAPTURE_ELEMENTS)
    rows.append({"name": "GStreamer", "version": version.splitlines()[0] if valid and version else "Unknown",
                 "status": "Runtime elements verified — live capture not tested" if not missing else
                 "Missing/incomplete runtime: " + ", ".join(missing)})
    devices = []
    try:
        for entry in sorted(video_root.glob("video*"))[:32]:
            try:
                devices.append(entry.joinpath("name").read_text().strip()[:120])
            except OSError:
                devices.append(entry.name)
    except OSError:
        pass
    rows.append({"name": "HDMI capture", "version": "Not configured",
                 "status": ("Detected video devices: " + ", ".join(devices) + ". Compatibility and signal require setup.")
                 if devices else "No video devices detected. This does not prevent Isolde from running."})
    return {"components": rows, "captureConfigured": False, "missingElements": missing}


if __name__ == "__main__":
    print(json.dumps(inspect()))
