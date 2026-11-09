# HDMI capture and input switching

Isolde's capture path uses one USB/UVC video capture card and an optional RS-232-controlled HDMI switch. The switch chooses which upstream HDMI source reaches the card. Isolde previews the card's video inside its own interface; it does not open a desktop capture application.

## Before setup

1. Connect the HDMI sources to the switcher's numbered inputs.
2. Connect the switcher's video output to the capture card's HDMI input.
3. Connect the capture card to the Isolde computer by USB.
4. Connect the switcher's RS-232 control port to the computer with the correct serial cable or USB-to-serial adapter. Some products require a null-modem cable; check the switch manual.
5. Connect the capture card's audio output to the Isolde audio input only if the card exposes a separate audio device and you want audio monitoring.

Do not connect an HDMI output back into the switch input. Use the switch and capture-card manuals to confirm port direction and power requirements.

## Configure in Isolde

Open **Settings → System → HDMI Inputs** with the remote. Device, format, audio input, serial port/rate, command, selected input, input-name, and session appearance changes are staged locally. Left/Right changes choices without persisting them. **Preview staged devices** tests the current draft's video card, format, and optional audio input without saving or sending a switch command; select it again after changing a device. **Apply capture changes** writes the complete validated configuration in one settings sync; **Cancel capture changes** or Back stops preview and discards the draft. Sending a test switch applies the draft first.

The editor submits versioned `capture-apply`, `capture-switch-input`, preview, cancel, and refresh commands through `ActionDispatcher`. `ComponentSchema::editorAction` rejects unknown fields, unsupported command variants, malformed labels, out-of-range HDMI inputs, and invalid serial settings before dispatch. The capture controller then checks discovered-device availability, protocol validity, and atomic persistence; schema acceptance alone never asserts that hardware is present or supported.

1. Choose **Capture card**. Compare the device name and listed resolutions/frame rates with the label or manual for the card. A device appearing in this list is not proof that it is an HDMI capture device.
2. Optionally choose **HDMI audio** if the card exposes a matching audio input. Audio monitoring requires a Qt Multimedia backend that supports capture-session audio output; older Qt builds remain video-only.
3. Choose the **RS-232 switch port**. Disconnect other serial adapters first if their names are difficult to distinguish.
4. Set the **Serial speed** to the switch manual's baud rate. The defaults are 9600 baud, 8 data bits, no parity, and one stop bit.
5. Choose a **Command format** only if it matches the switch manual. The generic examples are raw input number plus carriage return, raw number plus CR/LF, or `HDMI ` plus number plus carriage return. These are examples, not universal protocols.
6. Use **Command template** to enter the exact command, with `{input}` where Isolde should insert the selected number. Supported escapes are `\r`, `\n`, and `\t`; commands are limited to printable ASCII and 128 bytes. Isolde sends only these bytes through the selected serial port. It does not run shell commands.
7. Rename the selected input to a household-friendly name such as “Game console”. Labels must be nonempty, at most 40 characters, and contain no control characters. Input labels and configuration are stored on the device and shared across Isolde profiles.
8. Choose **Preview staged devices** before sending a switch command. This tests the selected devices without changing the switch.
9. Select **Send test switch** only after checking the port, baud rate, command, and selected input. A completed serial write means only that bytes were sent. Confirm that the preview visibly shows the expected device.

On the default Home screen, HDMI 1 through HDMI 8 are visible but unavailable until a capture card, connected serial port, and valid switch command are configured. To add or rename a shortcut, use **Customize Home → Add tile → HDMI input**. Enter an input number from 1 to 8 with the on-screen keyboard.

## Session controls and status

From an available Home input tile, Isolde sends the configured command and opens the full-screen preview. Apps declared as capture surfaces open the same eight-input picker first, without guessing which physical input the app uses. Left/Right chooses another input in a direct session; OK sends that input's command. After a switch, the heading remains **Verifying input** until video frames arrive and the user confirms the visible picture with OK. Back or Home stops capture and restores focus to the originating tile.

Choose Instant, Fade, Slide, or Expand and a 200–350 ms duration in HDMI Inputs; the global Reduced Motion setting makes these transitions instant. Expand is the default and uses the visible originating Home or page tile bounds when available, falling back to a centered expansion otherwise. While the picture is masked, an HDMI shortcut's existing managed custom tile artwork can be shown for its matching input; the originating shortcut takes priority, then matching shortcuts on the current page or Home. Otherwise a neutral background is shown. Switching to another input never reuses artwork belonging to the original input.

For independent fallback images, open **Input fallback artwork** in HDMI Inputs. Left/Right chooses among images already imported to this profile through Home Studio, separately for each input; the right pane previews the draft. Select **Apply artwork mappings** to save all mappings together, or Back/Cancel to discard them. These profile-local images take priority over shortcut artwork. A removed or missing image falls back to shortcut artwork or a neutral background.

If the serial write fails or times out after it was queued, the session reports the failure and OK retries the selected input. Menu closes the session and opens HDMI Inputs configuration; Back returns to the originating Home item. The picture is masked while a command is pending, and confirmation is unavailable until the serial write has completed and a subsequent video frame has arrived; a completed write or subsequent frame still does not prove that the switch changed inputs.

Status distinguishes missing/unselected capture devices, a disconnected serial port, missing or invalid switch protocol, serial errors, waiting for video, arriving frames, and stopped preview. Capture/audio hotplug is observed by Qt Multimedia; available serial ports are polled every two seconds for reconnect. The capability report separately identifies the Qt Multimedia capture/audio path, Qt SerialPort adapter, discovered device presence, preview surface, and whether the Qt build supports capture-session audio monitoring. Disconnecting a capture device stops preview; a serial timeout/error is retryable. Back during an incomplete serial write cancels the queued write, but partial bytes may already have reached the switch, so check the switch before retrying. Video frames do not prove that the selected HDMI source is correct. The captured content is never labeled as the requested source until the user has visually checked it.

## Limits and hardware acceptance

The switch protocol is model-specific. Isolde does not guess a protocol, infer that a switch changed input from a successful serial write, or bypass HDCP. Protected sources may intentionally produce no image. A black or stale image must be checked at the source, switch, card, and cable.

The current software foundation discovers devices, previews via Qt Multimedia's GStreamer-backed camera path, routes audio where the installed Qt version supports it, sends bounded asynchronous RS-232 commands, and reports frame arrival/loss. No capture card or switch is available in the development environment, so device compatibility, input mapping, actual signal detection, latency, resolution changes, frame pacing, audio sync, hotplug, and long-duration operation remain unqualified. Test these on the intended hardware before advertising the inputs as supported.

Video capture and audio monitoring use independently injectable adapters; the supplied Qt adapters share a Qt capture-session bridge, and Qt versions before 6.8 are explicitly video-only. Native and installed virtual-device journeys have not validated these paths.

## H04 hardware matrix and numeric procedure

Record one matrix row per tested combination; never generalize a passing row to an untested device or firmware:

| Area | Required identity and configuration |
| --- | --- |
| Host | Computer/mainboard revision, BIOS, Ubuntu release, kernel, Qt Multimedia and GStreamer/plugin versions, Isolde revision |
| Capture | Manufacturer/model/revision/firmware, USB VID:PID and connection path, selected video format/resolution/pixel format/frame-rate range, audio device identity |
| Switch | Manufacturer/model/revision/firmware, cable/adapter and null-modem status, serial adapter identity, baud/data/parity/stop/flow settings, exact documented command template |
| Signal path | Source model/output mode per each of eight inputs, HDCP state, switch output, cable lengths, capture-card input, selected Isolde audio output |

For each matrix row, record timestamps and counts, not pass/fail adjectives alone:

1. Run 30 switch trials on each of the eight inputs (240 total). For each, record Isolde's serial-write-completion latency and first-frame-callback latency in milliseconds, whether the expected source was independently visible, and any timeout/error. Report per-input success/error counts and p50/p95/max latencies. A write-completion or frame-callback timestamp is not proof of correct routing.
2. Keep the stream active for five minutes at every claimed capture format. Record requested/observed format, average reported frame rate, maximum inter-frame gap in milliseconds, no-signal/recovery events, and any audio dropout. The UI's frame callback metrics are diagnostic samples; validate actual displayed content independently.
3. Measure audio/video synchronization with 30 visible flash + audible click events per claimed format. Record signed offset in milliseconds for every pair and report min/median/max and drift over the five-minute run. Use the same physical output path and state the measurement instrument and uncertainty.
4. Perform ten unplug/replug cycles each for capture, serial, and selected audio devices. Record disconnect detection and usable-recovery times in milliseconds, whether a refresh/reselection was needed, and whether preview/routing recovered without restarting Isolde.
5. Preserve the exact matrix row, raw sample set, calculated summaries, protocol manual reference, and unresolved failures together. Establish numeric acceptance limits before qualification; this procedure does not invent a hardware-performance budget or mark any device qualified.

The setup screen exposes monotonic diagnostics for serial request-to-`bytesWritten` completion, request-to-first subsequent video-frame callback, average callback rate over one-second windows, and maximum observed inter-frame callback gap for the latest preview. They are sample timings, not a serial acknowledgement, source-recognition result, display scan-out measurement, dropped-frame detector, or audio/video-sync measurement. The test record must keep those distinctions.

If the card or serial port is visible but Isolde cannot open it, have the system administrator grant the Isolde service account the required device permissions. Do not run Isolde as root. Installation does not silently alter host security groups or permissions.