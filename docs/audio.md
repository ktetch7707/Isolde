# Audio and volume strategy

Isolde should be able to own day-to-day living-room volume while the TV stays at a stable middle level. The recommended household setup is:

1. Set the TV or soundbar to a comfortable mid-level reference volume.
2. Leave the TV volume mostly alone during normal use.
3. Use the Isolde remote, controller, keyboard shortcut, or app overlay for volume up, volume down, and mute.

This avoids bouncing between the TV remote, keyboard, desktop mixer, and app-specific volume controls. It also matches the appliance thesis: Isolde is the smart-TV brain, so sound should feel like part of Isolde rather than part of the host desktop.

## Volume ownership modes

Audio Settings currently supports only `Isolde output`. Unsupported modes are not selectable:

- `Isolde output`: preferred default. Isolde controls the system/application output volume and the TV remains at the configured reference level.
- `TV / HDMI-CEC`: future mode for TVs, receivers, or soundbars where HDMI-CEC volume commands are the correct path.
- `Hybrid`: future mode where Isolde controls its own output but can also forward volume/mute events to HDMI-CEC equipment.

The current implementation uses asynchronous PipeWire (`wpctl`, preferred) or PulseAudio (`pactl`) commands and reads back volume/mute before updating the displayed values. Each command has a two-second deadline. Rapid input is coalesced and the system output is polled every three seconds for external changes. A failed command leaves the last confirmed values intact and marks the backend unconfirmed. This is system output control, not per-app volume or HDMI-CEC forwarding.

Startup applies Remember last, Fixed, or Start muted once per application process. Start muted sends mute before changing volume. A shared audio controller survives profile switching, so changing profiles does not reset the output. Settings use an explicit organization namespace and report save failures. Keyboard/HID volume commands work within the shell; compositor interception and foreground native apps still need hardware acceptance. HDMI-CEC TV power commands are exposed through Remote Settings; HDMI-CEC volume forwarding remains future work.

Automated mixer tests use isolated subprocess doubles, not the machine's real volume. They cover both command formats, readback, coalescing, mute, external changes, persistence without an application organization, all startup policies, failures, malformed replies, and nonblocking timeouts. A virtual sound card and confirmed mixer values do not establish that a person can hear sound through the physical TV.

VM acceptance (October 2026): VirtualBox audio had been disabled, with no ALSA sound cards and only Dummy Output. After a clean guest shutdown, HDA/default host audio output was enabled with microphone input left disabled. Linux now reports HDA Intel and Built-in Audio Analog Stereo. Live UI volume-up changed the mixer from 50% to 55%; mute read back as muted; an external 40% change appeared in Settings. Isolde was manually started after automatic guest login; dedicated boot-to-Isolde remains the separate pinned installation task. Eight test suites pass. Startup retries a temporarily unavailable audio service up to ten times at three-second polling intervals, then leaves errors visible and continues read-only polling.

## Choosing a sound output

Settings → System → Audio now includes Sound Output and Refresh Outputs. With WirePlumber (`wpctl`), left/right previews discovered playback outputs and Enter applies the selection. The active marker is read back from the system rather than optimistically changed. The output list refreshes during normal polling and excludes capture sources and video nodes. Output changes leave the destination's existing volume untouched. Already-playing applications may need playback restarted; forced stream migration is not implemented. Bluetooth pairing and HDMI card-profile/port configuration remain separate work. PulseAudio-only installations retain volume/mute support but do not yet offer this selector.

Regression tests cover two outputs, active-default confirmation, a command that succeeds without changing the default, unplugged outputs, and rejection of microphone/video IDs. Hardware switching across real TV/USB/Bluetooth devices still needs acceptance testing. Numeric node IDs are runtime discovery values, not persisted device identities; WirePlumber owns default-device persistence.

The actual VM Settings selector was visually verified showing Built-in Audio Analog Stereo as active. Applying that available output preserved its 50% level. All eight suites passed. The VM has only one playback output, so selecting between two devices is currently covered by the isolated mixer double, not claimed as a physical-device test.

## Built-in sound test

Settings → System → Audio → Test Sound plays a generated three-second stereo WAV: left, right, then both, with quiet tones and short fade-ins/outs. Enter starts/stops it; leaving Audio Settings stops it. The test uses `pw-play` or `paplay` and the system default output, does not change volume/mute, and asks the user to unmute first when needed. Output switching is blocked during the test. An eight-second watchdog bounds player failure; temporary audio is removed after playback. The completion message asks whether sound was heard rather than claiming speaker audibility.

Automated tests validate WAV format, length, low peak amplitude, left/right channel separation, successful completion, stop, missing-player errors, and preservation of mute/volume. Real hardware audibility remains a user acceptance check.

VM playback stall resolved (October 15): both Isolde's test and a standard Ubuntu sound file stalled with the virtual HDA controller. ALSA showed RUNNING but its hardware pointer stayed at zero. Changing only the host backend did not resolve it. After clean shutdown, switching this VM to AC97 / STAC9700 with the default host audio backend restored sample consumption: hardware pointers advanced from 44761 to 93670, and repeated three-second stereo probes completed in 3.12, 3.08, and 3.04 seconds. Output remains enabled and microphone input disabled. This isolates the workaround to the VM audio-controller path; it does not establish the underlying VirtualBox/driver defect or certify physical speaker audibility.

Run `XDG_RUNTIME_DIR=/run/user/1000 python3 tools/audio-playback-check.py` in the guest for the real playback integration probe. It generates the same quiet left/right/both signal, bounds playback to eight seconds, reports completion/timeouts, and removes its temporary WAV. Unlike CTest doubles, it exercises the real audio service and makes sound. Do not substitute simulated-player success for this check.

User acceptance: on October 15 the user was in the room and confirmed hearing the test after the AC97 change. This verifies audibility for the current VM/host speaker setup, not all future TVs or receivers.

## TV reference level

`TV Reference Level` is a setup hint and calibration target, not a second active mixer. It represents the volume level the user should set on the TV or soundbar during initial setup. A typical default is 50%.

The finished first-run flow should include a guided test:

1. Play a steady test tone or sample clip from Isolde.
2. Ask the user to set the TV/soundbar near the reference level.
3. Confirm that Isolde volume at 50% sounds comfortable.
4. Save that reference so future support instructions can say “return the TV to your Isolde reference level.”

## Backend roadmap

Production audio should add:

- output hotplug and hardware-specific acceptance for PipeWire/PulseAudio;
- PulseAudio-only output selection, HDMI profiles/ports, and explicit existing-stream migration;
- HDMI-CEC volume/mute forwarding where hardware supports it;
- per-session audio ownership and ducking policy;
- night mode/compression backend;
- lip-sync delay backend;
- visible failure states when the mixer, HDMI device, or CEC adapter is unavailable.

The UI should never claim it changed hardware volume unless the backend confirms it.
