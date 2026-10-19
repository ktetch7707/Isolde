# Disposable playback acceptance

These files are development fixtures for the previously created synthetic Kodi
profile `9ba54c11-e4b3-4d4c-a2a5-55d40a221054`, not consumer setup defaults.
Never copy them over normal user settings. The test runs with a separate
`XDG_CONFIG_HOME=/tmp/isolde-playback-acceptance`, while using that disposable
profile's existing Kodi database and read-only synthetic Videos collection.
`profiles.json`, `device-setup.json` and `MediaAccess.conf` belong under its
`Isolde` directory. The NFO accompanies a generated two-minute Theora test clip.

Run the built app with the normal graphical session environment, stop the normal
user service only for the test, and restore it with a shell EXIT trap. Check
direct play, pause, seek, Back, Home, natural completion, and failed start.
The read-only/status probe is `tools/kodi-rpc-probe.py`; it also permits an
explicit library scan. Production code must never depend on these fixture IDs.
