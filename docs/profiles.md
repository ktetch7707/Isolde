# Profiles

Settings → Profiles supports creating, renaming, and switching profiles using the remote and on-screen keyboard. New profiles start with the default Home layout. Creating a profile does not copy another person's browser data. The last selected profile is restored on launch.

The initial **Household** profile preserves the existing Home layout and ambient preferences. Names can change without changing a profile's stable identifier or storage. Web apps use newly named persistent browser storage; previous temporary browser sessions are not migrated, so a first sign-in may be necessary.

## Isolation contract

Each profile owns its Home tiles, categories, theme, background, widget configuration, ambient preferences, and managed slideshow directory. Each has a separate Chromium storage identity for web apps: cookies, local storage, IndexedDB, cache, and other browser profile data. Sign-ins persist subject to the service's own expiration and authentication policies. Isolde does not store account passwords itself.

Switching exits the current session and destroys the entire QML engine, web pages, and browser profile before creating the next profile's interface. Browser storage names remain fixed for the lifetime of each engine; a live page is never pointed at another user's storage. Installed packages, network, volume, display, remotes, and detected-source policy remain device-wide.

Native Linux applications now launch through an enforced per-profile, per-app filesystem and process sandbox. Each gets a private home, XDG config/data/cache/state folders, temporary directory, process namespace, and a fresh private session bus. The host home, other profiles, host keyring, agent sockets, and host session/system buses are not mounted. Existing desktop accounts are never automatically copied, including into Household; users sign in once in each new native profile. Names can change without moving these folders.

The native session host first asks the app to shut down and save state, then terminates remaining descendants. Closing Isolde or losing its process also tears down the sandbox. Normal native app closure returns Home without a false launch-failure message. Missing isolation dependencies or unsupported launchers produce an error instead of a shared-account fallback.

Display, audio, GPU, installed system files, and internet connectivity are shared deliberately. Local file/folder actions grant the selected target read-only at `/media/selected/<name>`; profile/account directories and broad system roots are refused. An app's file browser initially sees its own private home, not the user's entire desktop home. Native apps requiring an external browser login or a host keyring/portal need a dedicated integration; Spotify's phone/QR sign-in screen is available. Snap/Flatpak launcher delegation is blocked pending a dedicated adapter. Steam has not yet been compatibility-tested.

This provides independent native account storage and lifecycle, not authenticated OS users or a complete hostile-app security boundary. In particular, X11 display access and local network services remain shared. Two people deliberately signing into the same online account can still see server-side history shared by that service.

### Native runtime details

Storage lives under the application data directory at `native-profiles/<profile-id>/<app-id>/home`. A readonly system filesystem and this one writable home are assembled by bubblewrap. The helper `isolde-native-host` must be installed next to Isolde. `config/native-session-bus.conf` creates a bus on a filesystem socket inside the private `/tmp`, with no host activation services. Its AppArmor bus mediation is disabled because the private namespace has readonly securityfs; this does not modify the host's AppArmor configuration, and the host bus is never exposed. The configuration is based on the [D-Bus daemon specification](https://dbus.freedesktop.org/doc/dbus-daemon.1.html) and [bubblewrap lifecycle options](https://github.com/containers/bubblewrap/blob/main/bwrap.xml).

## Verification

`ctest --output-on-failure` includes personal-layout/ambient persistence tests and `profile_browser_isolation`. The browser test uses the production browser-profile QML component with temporary storage: A writes cookies/localStorage/IndexedDB; B verifies empty storage and writes its own values; A and B each recover only their own values. It runs both engine teardown/recreation within a process and separate browser processes. Session storage must start empty. No real service credentials are used.

Manual acceptance: create a profile, switch to it, change its Home layout, switch back, and confirm the old layout. Sign into separate YouTube accounts in two profiles, then switch and restart Isolde to confirm each service's account session. Real YouTube authentication remains a manual account-based test.

`native_profile_isolation` exercises actual Linux sandboxes: separate profile/app storage, private working D-Bus connections, hidden host data and agents, persistence on reopening, read-only media grants, invalid IDs and symlink refusal, graceful state saving, and cleanup of detached descendants after forced and normal exits. Tests use temporary fixture data, never account credentials.

For controlled VM checks of installed native apps: `isolde_native_smoke APP PROFILE_ID SECONDS` uses the production DesktopSession, then requests exit (maximum 60 seconds). Its PASS reports process lifecycle only; always pair it with an actual screenshot/window check. Kodi and Spotify have been visually opened in the isolated runtime. Real authenticated account switching remains a user-account acceptance test.

## Remaining work

- Steam and other app compatibility testing; dedicated Snap/Flatpak, login callback, and keyring/portal integrations without access to another profile's account.
- Separate OS users or stronger display/network isolation if authenticated, adversarial profile security is required.
- Optional profile PINs, startup chooser, guest sessions, and parental controls.
- Profile deletion with explicit confirmation and removal of all owned storage.
- Per-app account reset and isolated additional instances within one profile.
