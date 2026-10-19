# Remote compatibility and setup plan

Isolde must work with a physical remote in a way that feels friendly to a normal living-room user. A keyboard is acceptable for development and recovery, but not as the primary product experience.

## Compatibility targets

Isolde should support, in order of usefulness:

1. USB or 2.4 GHz remotes that present as keyboard/HID devices.
2. Bluetooth remotes and game controllers.
3. HDMI-CEC remote input from the TV, when supported by the host hardware or adapter.
4. IR receivers such as FLIRC-style USB adapters.
5. Gamepads as first-class navigation devices, especially for game mode.

The minimum viable remote has: Up, Down, Left, Right, OK, Back, and Home. Enhanced remotes may add Volume, Mute, Play/Pause, Rewind/Fast Forward, Menu, Search, microphone, colored buttons, and app shortcuts.

The preferred plug-and-play product path is a known-good USB 2.4 GHz/HID remote receiver. It should work before network setup, avoid Bluetooth pairing during first boot, and let Isolde detect ordinary keyboard/media-key events immediately. Bluetooth, HDMI-CEC remote input, and IR receivers are important compatibility paths, but the default user experience should not depend on them.

## User-friendly setup flow

Remote management should be a first-class Settings area, not a hidden support tool. It should be available from Settings → Remotes and should also appear during first-run onboarding.

The finished flow should be:

1. Choose remote type: simple TV remote, media remote, gamepad, HDMI-CEC, or advanced receiver.
2. Pair or detect the device.
3. Test required buttons one at a time.
4. Offer automatic mapping when the device emits standard keyboard/media keys.
5. Ask the user to remap only missing or incorrect buttons.
6. Save a named profile.
7. Provide a rescue shortcut: any keyboard can still navigate and open Settings.

## Mapping philosophy

Remote inputs should be translated into Isolde semantic commands such as Navigate Up, Confirm, Back, Home, Volume Up, Volume Down, Mute, Play/Pause, Search, Menu, and Game Overlay. QML should not depend on raw key names or device-specific scan codes.

This keeps the product flexible: a Bluetooth remote, TV CEC remote, gamepad, and keyboard can all drive the same shell behavior.

## Troubleshooting requirements

Remote Setup should include:

- “Test buttons” mode that shows the last recognized input;
- clear explanation when no input is detected;
- profile reset;
- button remapping;
- device battery/status when available;
- Bluetooth pairing help;
- HDMI-CEC compatibility notes;
- a way to export diagnostics for remote support.

Current implementation status: Isolde has a top-level Settings → Remotes section backed by a persisted Remote Settings model and TV page for setup mode, profile selection, button-test status, power-button behavior, HDMI-CEC status, and TV on/standby test actions. The global input vocabulary now includes volume up/down, mute, power, play/pause, menu, and search in addition to D-pad, OK, Back, and Home. While setup mode is active, recognized semantic commands are reflected as the last tested button. HDMI-CEC uses safe `cec-client` detection and explicit TV power commands when available; if no CEC backend is present, Isolde reports that clearly and falls back to Ambient Mode behavior. Hardware discovery, Bluetooth pairing, evdev/libinput device identity, CEC event monitoring, and remapping are follow-on backends.
