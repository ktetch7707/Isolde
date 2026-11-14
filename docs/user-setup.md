# Isolde user setup guide — version 0.0.1.2

Isolde is a TV-first appliance shell for a dedicated computer. It is not a general desktop with an extra app: once set up, you use it with a remote from the couch, and Isolde owns normal navigation, volume, apps and playback. This guide walks you through obtaining the image, installing it, completing first-run setup, playing local media, and using updates and recovery.

This guide matches Isolde **0.0.1.2**. It ships with the installer and is readable offline. No programming, terminal or developer-account steps are needed for the ordinary product journey.

## Contents

1. [Before you begin](#1-before-you-begin)
2. [Write the image to USB](#2-write-the-image-to-usb)
3. [Install Isolde](#3-install-isolde)
4. [First-run setup wizard](#4-first-run-setup-wizard)
5. [Profiles, media library and apps](#5-profiles-media-library-and-apps)
6. [Local playback](#6-local-playback)
7. [Everyday use](#7-everyday-use)
8. [Updates, rollback and recovery](#8-updates-rollback-and-recovery)
9. [Troubleshooting](#9-troubleshooting)
10. [What is preserved or erased](#10-what-is-preserved-or-erased)

## 1. Before you begin

### What you need

- A computer dedicated to Isolde (it will be erased and used only for Isolde).
- A USB flash drive, 16 GB or larger (it will be erased).
- Another computer (Windows, macOS or Linux) to write the image to the USB drive.
- A TV or display connected by HDMI, and a remote (Bluetooth, HDMI-CEC, IR or USB). During setup you may also use a USB keyboard.

### Download and check the image

Download the Isolde appliance image for 0.0.1.2 from the product download page. The download page also lists the **SHA-256 checksum** for the image file.

After downloading, verify the checksum before writing the image:

- **Windows:** open PowerShell in the download folder and run  
  `Get-FileHash -Algorithm SHA256 Isolde-0.0.1.2.img`  
  Compare the printed hash with the one on the download page. They must match exactly.
- **macOS:** run `shasum -a 256 Isolde-0.0.1.2.img` in Terminal.
- **Linux:** run `sha256sum Isolde-0.0.1.2.img`.

If the checksum does not match, delete the download and download it again. Do not install an image whose checksum does not match.

### Disk-erasure safety

Installing Isolde erases **everything** on the computer disk you choose for it. Back up any personal files from that computer first. During installation you must explicitly select the target disk; Isolde never picks a disk for you and never formats a disk you did not choose. Disks you do not select are left untouched.

## 2. Write the image to USB

1. Insert the USB flash drive into the other computer.
2. Open your operating system's image writer (for example, the built-in "Restore disk image" or "Write disk image" tool, or a free USB importer of your choice).
3. Select the Isolde image file you downloaded and verified.
4. **Explicitly select the USB flash drive as the destination.** Confirm it is the drive you intend to erase — its size and label should match your USB stick.
5. Start the write. Wait for it to finish and for the tool to report success. Do not unplug the drive while writing.
6. Safely eject the USB drive and move it to the computer you want to install Isolde on.

## 3. Install Isolde

1. Turn off the target computer. Insert the USB drive.
2. Turn the computer on and open its boot menu (the key is shown briefly on screen at start; common keys are F12, Esc, F2 or Del depending on the computer).
3. Choose the USB drive as the boot device. The Isolde installer starts.
4. **Choose language and region** when asked, then continue.
5. The installer shows the disks in the computer. **Choose the disk you want to dedicate to Isolde.** A confirmation screen reminds you that this disk will be erased. Select **Erase selected disk and install Isolde** to continue, or **Back** to choose a different disk or cancel.
6. The installer copies the system and configures it. This can take several minutes. A progress bar and the current step are shown; do not turn the computer off during installation.
7. When installation finishes, the installer reports success and asks you to restart. Remove the USB drive when prompted, then select **Restart now**.

### First boot

After restart, Isolde starts automatically. The first screen you may see briefly is "Isolde is starting — waiting for display." This is normal: Isolde waits up to 30 seconds for the display to be ready before opening the shell, so a slow TV or receiver does not cause a failed start. Wait for the setup wizard to appear.

## 4. First-run setup wizard

Complete the wizard with remote directional buttons and **Confirm** (or **OK**/**Enter**); use **Back** to return to the previous screen. A USB keyboard also works during setup.

### Screen 1 — Language and region

Choose your language and region with the directional buttons and press **Confirm**. The wizard text updates to your language. Expected result: all following screens appear in the chosen language.

### Screen 2 — Display safe area and UI scale

A test pattern with a margin is shown. Adjust the margin with the directional buttons so the pattern fits inside your TV's visible area, then choose the UI scale (smaller / default / larger). Press **Confirm**. Expected result: the Home screen and menus no longer disappear behind the TV bezel, and text is readable at TV distance.

### Screen 3 — Audio

Set the TV (or soundbar) to a stable mid-level reference volume. In Isolde choose the volume owner (TV/soundbar or Isolde) and run **Test volume** and **Test mute**. Press **Confirm**. Expected result: you hear the test tone and muting works from the remote.

### Screen 4 — Pair or test the remote

Choose your remote type (Bluetooth remote, HDMI-CEC TV remote, IR receiver or USB keyboard) and follow the on-screen pairing or test instructions. Press **Confirm** when the remote moves the highlight. Expected result: pressing directional buttons moves the highlight on screen.

### Screen 5 — Network

Choose Wi-Fi or wired Ethernet. For Wi-Fi, select your network and enter the password with the on-screen keyboard. Press **Confirm**. Expected result: the network indicator shows connected and the time updates from the network.

### Screen 6 — Privacy and update defaults

Review whether Isolde may check for updates and send anonymous error reports. Choose your defaults and press **Confirm**. These can be changed later in Settings.

### Screen 7 — Add first apps and services

Choose the first apps and services for Home, such as Kodi (local media), Plex, Home Assistant, YouTube, Spotify, Steam or local folders. Select each tile you want and press **Confirm**. You can add or remove tiles later.

### Screen 8 — Confirm Home layout and finish

Review the Home layout. Press **Confirm** to finish setup. Expected result: Isolde opens to Home with your chosen tiles, ready for remote use.

If you back out of the wizard before finishing, Isolde reopens the wizard at the next start until setup is complete.

## 5. Profiles, media library and apps

### Profiles

Isolde keeps settings, Home layout and app data separate per profile. Choose your profile at start; each profile has its own layout and library sources. Do not share your profile with other people in the household — each person should use their own profile.

### Media library (Kodi)

Open Kodi from Home to add local folders of video, music and photos. Kodi scans the folders you select and builds the library. Isolde reads Kodi's library for posters and playback; Isolde does not keep a separate media database.

### Apps

Installed apps appear as tiles on Home. Select a tile to open the app. Apps that support returning to Home show a **Return to Isolde** option (or use **Back** on the remote).

## 6. Local playback

1. From Home, open Kodi or select a media tile.
2. Browse your library and select a video, music track or photo.
3. Playback starts in full screen. Use the remote transport buttons (play/pause, stop, skip) as shown on screen.
4. Press **Stop** or **Back** to end playback and return to where you were in Isolde.

Playback position is remembered per item, so resuming later continues from the same point.

## 7. Everyday use

- **Directional buttons:** move the highlight.
- **Confirm / OK:** open the highlighted tile or item.
- **Back:** return to the previous screen.
- **Menu / Search:** open options or search for apps and media.
- **Home button:** return to Home from anywhere.
- Powering the TV off and on resumes Isolde where it left off; Isolde does not need to be reopened after the TV sleeps.

## 8. Updates, rollback and recovery

### Updates

When an Isolde update is available, Settings shows **Update available**. Select **Install update**. The new version is downloaded, checked and installed alongside the current version; the running Isolde is not interrupted. After installation, restart Isolde to use the new version. Your profiles, layouts, media library and saves are preserved.

### Recovery screen

If Isolde cannot stay running after repeated attempts, automatic restarts pause and the **Isolde Recovery** screen opens:

- **Retry Isolde** — try starting the current version again. No internet is needed.
- **Use previous release** — shows **Confirm previous release** when a previous installed version exists. Selecting it switches to the previous version and restarts Isolde. Your profiles, settings, layouts, media and saves are not reset. This does not undo operating-system updates or data migrations.
- If no previous release is available, the screen reports that and offers only **Retry Isolde**.

Use **Back** to cancel a confirmation and return to the recovery choices.

### Rollback

To roll back outside the recovery screen, open Settings → System → Recovery and choose **Roll back to previous release**, then confirm. Isolde restarts on the previous version. Rollback requires a previous release to exist; after a fresh install there is none yet.

## 9. Troubleshooting

This section covers the most common symptoms, what to try and what is preserved.

| Symptom | What to try |
|---|---|
| No image on the TV after first boot | Wait up to 30 seconds for "waiting for display" to finish. Check the TV input is set to the Isolde computer's HDMI port. Try a different HDMI port or cable. |
| No sound | Check TV/soundbar volume and mute. In Settings → Audio, confirm the volume owner. Run the volume test from first-run setup screen 3. |
| Remote does not move the highlight | Re-pair the remote in Settings → Remotes. For Bluetooth remotes, make sure the remote is in pairing mode. Try a USB keyboard to confirm the shell is responding. |
| No network | Check the cable or re-enter the Wi-Fi password in Settings → Network. Confirm your router has internet access from another device. |
| Installer will not start | Confirm the USB drive was selected as the boot device and the image checksum matched before writing. Try writing to a different USB port or drive. |
| First-boot failure (Isolde does not open after install) | Let the first boot complete "waiting for display." If it still fails, restart once. If it keeps failing, the recovery screen offers the previous release or retry. |
| App does not open | Confirm the app tile is installed and, for online apps, that network is available. Open Settings → System → Components to confirm required media pieces are present. |
| Update fails to install | Check network and free disk space. Retry the update. A failed update does not replace the running version. |
| Isolde keeps restarting | The recovery screen opens automatically. Choose **Retry Isolde** or **Confirm previous release** to roll back. Profiles and layouts are preserved. |

For anything not listed, open Settings → System → Diagnostics and choose **Share safe diagnostics** to create a support package. The package contains logs and version information only — no personal media or account credentials.

## 10. What is preserved or erased

| Action | Erased | Preserved |
|---|---|---|
| Writing image to USB | Everything on the USB stick | Nothing on the computer |
| Installing Isolde | Everything on the **selected** computer disk | Disks you did not select; your downloaded image file |
| First-run setup | Nothing | All choices you make |
| Update | Nothing | Profiles, layout, library, saves, settings |
| Rollback / recovery | Nothing | Profiles, layout, library, saves, settings |
| Reinstalling Isolde | Everything on the selected disk | Disks you did not select |

### Version note

This guide describes Isolde **0.0.1.2**. If your installed version differs, some screens or options may vary. Check Settings → System → About for your exact version, and use the guide that shipped with that version.
