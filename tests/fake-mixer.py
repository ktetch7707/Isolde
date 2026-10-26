#!/usr/bin/python3
"""Deterministic mixer double. Never accesses the system audio service."""
import json
import pathlib
import sys
import time

root = pathlib.Path(__file__).parent
state_file = root / 'state.json'
state = json.loads(state_file.read_text()) if state_file.exists() else {'volume': 50, 'mute': False}
mode = (root / 'mode').read_text() if (root / 'mode').exists() else ''
if mode == 'fail':
    sys.exit(1)
if mode == 'hang':
    time.sleep(10)
if mode == 'malformed':
    print('not a volume')
    sys.exit(0)
command = sys.argv[1]
with (root / 'commands').open('a') as log:
    log.write(' '.join(sys.argv[1:]) + '\n')
if command == 'status':
    active = str(state.get('output', 55))
    print('Audio\n ├─ Sinks:')
    for ident, name in [('55', 'TV HDMI'), ('77', 'USB Speakers')]:
        if mode == 'unplug' and ident == '77':
            continue
        print(f" │  {'*' if ident == active else ' '}   {ident}. {name} [vol: 0.50]")
    print(' ├─ Sources:\n │ * 88. Microphone [vol: 1.00]\nVideo\n ├─ Sinks:\n │ 99. Camera [vol: 1.00]')
elif command == 'set-default':
    if mode != 'ignore-selection':
        state['output'] = int(sys.argv[2])
elif command in ('set-volume', 'set-sink-volume'):
    state['volume'] = int(sys.argv[3].rstrip('%'))
elif command in ('set-mute', 'set-sink-mute'):
    state['mute'] = sys.argv[3] == '1'
elif command == 'get-volume':
    print(f"Volume: {state['volume'] / 100:.2f}" + (' [MUTED]' if state['mute'] else ''))
elif command == 'get-sink-volume':
    print(f"Volume: front-left: 32768 / {state['volume']}% / -18.06 dB")
elif command == 'get-sink-mute':
    print('Mute: ' + ('yes' if state['mute'] else 'no'))
# Queries must not overwrite a newer external adjustment with the state they
# read at process startup. The real mixer does not write volume on `status` or
# `get-volume`; doing so here raced confirmedReadAndFailure's external update.
if command in ('set-default', 'set-volume', 'set-sink-volume', 'set-mute', 'set-sink-mute'):
    state_file.write_text(json.dumps(state))
