# Declarative app packages

Isolde 0.0.1.2 introduces a deliberately constrained package format to prove App Store lifecycle behavior safely.

Packages may declare `handledActions` such as `open-folder`, `open-file`, `open-url`, or `open-disc`. Desktop packages that declare file/folder/disc support must also provide `actionHandlers` for each handled action. The first adapter forms are intentionally small: `{ "appendTarget": true }` appends the selected tile target as a single process argument, while `{ "arguments": ["--flag", "{target}"] }` appends explicit arguments with `{target}` expansion. This lets Isolde launch “open this disc/folder/file with this compatible app” without hard-coding Kodi, VLC, or any other handler into the shell.

The source manager uses the same declarations when it decides whether to show a contextual detected-source tile. If no installed app can handle the source's action type, the source is not shown as a dead tile.

## Manifest contract

Each curated package has a `manifest.json` with schema version, stable package ID, semantic version, minimum compatible Isolde version, publisher, user-facing metadata, declared capabilities, and one app definition. Package and app IDs must match and use the restricted lowercase ID format.

The app definition separates launch type from presentation mode. `type` describes how Isolde starts the app; `presentationMode` describes what the user should expect. Current presentation modes are `native-tv`, `web-kiosk`, `media`, `capture`, `game-hub`, `game`, `disc-media`, `disc-game`, `utility-desktop`, and `desktop-bridge`.

Packages may declare `networkRequirement` as `none`, `local-network`, or `internet`. This lets Isolde keep local capabilities available while dimming or disabling cloud-only apps when internet is unavailable. The default is `internet` for web packages and `none` for other package types, but curated packages should be explicit whenever user expectations matter.

Package selection should prefer the most TV-native viable variant. If a service offers both a living-room/web-kiosk experience and a general desktop client, the living-room route should be the primary Isolde package. The desktop client may remain available as a fallback or development bridge, but it should be labeled that way.

Game packages should prefer a controller-first living-room route. Steam, for example, should launch as a `game-hub` through Big Picture or an equivalent console-style surface rather than as the normal desktop client. Individual games and detected game discs should use `game` or `disc-game` so Isolde can eventually apply low-latency display, controller, audio, and return-home policies.

`utility-desktop` is reserved for an advanced Desktop Mode escape hatch. It is not a normal app package category and should not be used as a shortcut around TV-first app design.

Web-kiosk packages should use safe HTTPS or packaged `qrc` launch URLs and declare `allowedOrigins` for every required TV/auth origin. Isolde blocks navigation outside that list so a TV app cannot quietly degrade into a general-purpose browser session.

Local service packages, such as Plex and Home Assistant, should be exceptionally easy to add because they are central to Isolde's media-center role. A production package should support local-network discovery where possible, manual host/address entry where discovery fails, a TV-readable connection test, credential or account guidance, and editable tile placement. Prototype packages may point at public web entry points or demos only when that keeps development testable without pretending the setup flow is complete.

The catalog currently accepts `mock` packages and tightly validated `desktop` packages. Desktop packages may name a desktop-entry ID, such as `spotify`, plus an optional MPRIS bus name for graceful exit. They do not carry arbitrary shell commands. Desktop packages must be treated as development bridges unless they also prove a full-screen, remote-first, chrome-free smart-TV presentation.

## Installation lifecycle

1. Parse and validate the curated catalog index.
2. Parse every referenced manifest and reject the catalog if metadata, IDs, versions, compatibility, or colors are invalid.
3. Write the selected manifest beneath a same-filesystem staging directory.
4. Commit the manifest atomically with `QSaveFile`.
5. Rename staging into the versioned package directory.
6. Register the app and atomically persist activation state.
7. On failure, leave the previous active state unchanged.

Startup removes stale staging directories. An activation record is honored only when its versioned installed manifest exists. Uninstall first removes activation and the runtime app, commits state, and then removes that package's private directory.

## Trust boundary

The current local catalog is compiled and deployed with Isolde. It is not a network marketplace and cannot install native binaries, scripts, services, or arbitrary commands. Real desktop clients, such as Spotify, must already be provisioned by the trusted test environment or operating-system package manager before their Isolde package is useful.

Remote repositories require signed metadata, verified payload hashes, explicit capability consent, update rollback, repository trust policy, and isolation design before they may be enabled.

## Current Spotify notes

Spotify is the first real desktop-client experiment. The VM uses Spotify's Debian package because the Flathub package is community maintained and failed to launch reliably on the current guest stack. The Isolde Store install registers the Spotify launcher in Isolde; it does not yet download the OS package on a fresh machine.

Spotify's package is marked `presentationMode: desktop-bridge`. That is intentional: it proves real app launch mechanics, but it does not yet satisfy the finished smart-TV experience bar. The preferred future Spotify package should be an official TV/web-kiosk style integration if a viable route exists.

Kodi is the first non-standard media-center package used to verify Isolde can obtain and expose apps beyond mainstream streaming services. It is provisioned in the trusted VM from Ubuntu's `kodi` package and launched through an Isolde-owned desktop entry (`isolde-kodi`) that requests fullscreen. Its package is marked `presentationMode: media` because Kodi itself is living-room oriented, while the current launch boundary is still the desktop containment bridge. Current finding: Kodi launches successfully through Isolde, the bridge resolves `/home/isolde-dev/.local/share/applications/isolde-kodi.desktop`, starts `/usr/bin/kodi -fs`, finds the `Kodi.Kodi` window class, and applies fullscreen containment. Kodi's own first-run/add-on and empty-library prompts prove non-standard packages still need onboarding/first-run policy before they are considered appliance-clean.

Plex and Home Assistant are curated web-kiosk prototypes. Plex represents a household media-server integration. Home Assistant represents a local smart-home dashboard integration. The Home Assistant prototype currently uses the public demo dashboard so the App Store path can be tested without a configured local server; the finished package must ask for or discover the user's actual Home Assistant instance.

Desktop-bridge packages may also declare `windowClass` and `containment: fullscreen`. On Linux/X11 development systems, Isolde uses this metadata to request focus and fullscreen state for the launched window. Isolde resolves the declared desktop entry itself, validates that the entry is a non-terminal application, parses the Exec line without invoking a shell, rejects obvious shell-control syntax, starts the executable directly, and logs the resolved command path. Missing window-control tooling, a mismatched window class, or a process that exits without producing the declared window is treated as a containment failure to fix, not as proof that the app is TV-ready.

The Ubuntu test VM provisions a user-local `isolde-spotify.desktop` launcher that starts Spotify with Chromium's basic password store. This avoids GNOME's locked-login-keyring prompt in the autologin test session while leaving the keyring itself untouched.

## Storage

- Curated manifests: deployed `config/packages/<package-id>/manifest.json`
- Installed packages: application data `packages/<package-id>/<version>/manifest.json`
- Activation state: application configuration `installed-apps.json`

Installed capability and future Home placement remain separate concerns. The current prototype exposes installed packages on Home temporarily; the planned Home Layout System will make placement an explicit user-controlled decision.
