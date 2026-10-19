#!/usr/bin/env python3
"""Check real Linux playback completion, without changing volume or mute.

This is an integration diagnostic, not a CTest: it makes a quiet audible tone.
Successful player exit does not certify physical speaker audibility.
"""
import json
import math
import shutil
import struct
import subprocess
import tempfile
import time
import wave
from pathlib import Path


def main():
    player = shutil.which('pw-play') or shutil.which('paplay')
    if not player:
        print(json.dumps({'passed': False, 'reason': 'No playback command installed'}))
        return 1
    with tempfile.TemporaryDirectory(prefix='isolde-playback-check-') as directory:
        path = Path(directory) / 'stereo.wav'
        rate = 48000
        with wave.open(str(path), 'wb') as audio:
            audio.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
            data = bytearray()
            for frame in range(rate * 3):
                section, local_frame = divmod(frame, rate)
                t = local_frame / rate
                fade = max(0, min(1, t / .03, (.7 - t) / .03))
                sample = round(3276 * fade * math.sin(2 * math.pi * (660 if section == 1 else 440) * t))
                data.extend(struct.pack('<hh', 0 if section == 1 else sample, 0 if section == 0 else sample))
            audio.writeframes(data)
        started = time.monotonic()
        try:
            result = subprocess.run([player, str(path)], capture_output=True, timeout=8)
            passed = result.returncode == 0
            reason = 'Player completed; physical audibility not verified' if passed else 'Player failed'
        except subprocess.TimeoutExpired:
            passed, reason = False, 'Playback stalled beyond eight seconds'
        print(json.dumps({'passed': passed, 'reason': reason, 'elapsed_seconds': round(time.monotonic() - started, 2), 'player': player}))
        return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
