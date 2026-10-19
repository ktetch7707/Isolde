#!/usr/bin/python3
import pathlib
import struct
import sys
import time
import wave

with wave.open(sys.argv[1], 'rb') as sound:
    assert sound.getnchannels() == 2
    assert sound.getsampwidth() == 2
    assert sound.getframerate() == 48000
    assert sound.getnframes() == 144000
    samples = struct.unpack('<' + 'h' * 288000, sound.readframes(144000))
    assert max(abs(value) for value in samples) <= 3276
    assert any(samples[:96000:2]) and not any(samples[1:96000:2])
    assert not any(samples[96000:192000:2]) and any(samples[96001:192000:2])
    assert any(samples[192000::2]) and any(samples[192001::2])
pathlib.Path(__file__).with_name('validated-tone').touch()
time.sleep(0.3)
