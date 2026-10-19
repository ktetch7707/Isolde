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

Open **Settings → System → HDMI Inputs** with the remote.

1. Choose **Capture card**. Compare the device name and listed resolutions/frame rates with the label or manual for the card. A device appearing in this list is not proof that it is an HDMI capture device.
2. Optionally choose **HDMI audio** if the card exposes a matching audio input. Audio monitoring requires a Qt Multimedia backend that supports capture-session audio output; older Qt builds remain video-only.
3. Choose the **RS-232 switch port**. Disconnect other serial adapters first if their names are difficult to distinguish.
4. Set the **Serial speed** to the switch manual's baud rate. The defaults are 9600 baud, 8 data bits, no parity, and one stop bit.
5. Choose a **Command format** only if it matches the switch manual. The generic examples are raw input number plus carriage return, raw number plus CR/LF, or `HDMI ` plus number plus carriage return. These are examples, not universal protocols.
6. Use **Command template** to enter the exact command, with `{input}` where Isolde should insert the selected number. Supported escapes are `\r`, `\n`, and `\t`; commands are limited to printable ASCII and 128 bytes. Isolde sends only these bytes through the selected serial port. It does not run shell commands.
7. Rename the selected input to a household-friendly name such as “Game console”. Input labels and configuration are stored on the device and shared across Isolde profiles.
8. Choose **Start video preview** before sending a switch command. This tests the capture card without changing the switch.
9. Select **Send test switch** only after checking the port, baud rate, command, and selected input. A completed serial write means only that bytes were sent. Confirm that the preview visibly shows the expected device.

On the default Home screen, HDMI 1 through HDMI 8 are visible but unavailable until a capture card, connected serial port, and valid switch command are configured. To add or rename a shortcut, use **Customize Home → Add tile → HDMI input**. Enter an input number from 1 to 8 with the on-screen keyboard.

## Session controls and status

From an available Home input tile, Isolde sends the configured command and opens the full-screen preview. Apps declared as capture surfaces open the same eight-input picker first, without guessing which physical input the app uses. Left/Right chooses another input in a direct session; OK sends that input's command. After a switch, the heading remains **Verifying input** until video frames arrive and the user confirms the visible picture with OK. Back or Home stops capture and restores focus to the originating tile.

Status distinguishes missing/unselected capture devices, a disconnected serial port, missing or invalid switch protocol, serial errors, waiting for video, arriving frames, and stopped preview. Video frames do not prove that the selected HDMI source is correct. The captured content is never labeled as the requested source until the user has visually checked it.

## Limits and hardware acceptance

The switch protocol is model-specific. Isolde does not guess a protocol, infer that a switch changed input from a successful serial write, or bypass HDCP. Protected sources may intentionally produce no image. A black or stale image must be checked at the source, switch, card, and cable.

The current software foundation discovers devices, previews via Qt Multimedia's GStreamer-backed camera path, routes audio where the installed Qt version supports it, sends bounded asynchronous RS-232 commands, and reports frame arrival/loss. No capture card or switch is available in the development environment, so device compatibility, input mapping, actual signal detection, latency, resolution changes, frame pacing, audio sync, hotplug, and long-duration operation remain unqualified. Test these on the intended hardware before advertising the inputs as supported.

If the card or serial port is visible but Isolde cannot open it, have the system administrator grant the Isolde service account the required device permissions. Do not run Isolde as root. Installation does not silently alter host security groups or permissions.