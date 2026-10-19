# Architecture

## Boundaries

`AppRegistry` owns validated, stable app definitions loaded from JSON. QML receives a generic list and never switches on a particular app ID. `SessionManager` is the single session lifecycle authority. It creates an `AppSession` implementation from an app's type, rejects overlapping launches, and records the launching ID for focus restoration. `InputManager` converts keyboard events into semantic commands so a later remote backend can produce the same vocabulary.

The QML layer renders state and forwards intent. `Main.qml` explicitly selects either HOME or SESSION based on `SessionManager::active`; individual tiles contain no lifecycle behavior.

App definitions now separate launch mechanism from presentation promise. `type` describes the session implementation used by Isolde, such as mock, store, settings, or desktop. `presentationMode` describes the user-facing contract: `native-tv`, `web-kiosk`, `media`, `capture`, `game-hub`, `game`, `disc-media`, `disc-game`, `utility-desktop`, or `desktop-bridge`.

The architectural preference order is TV-first: use an official native-TV, living-room, web-kiosk/PWA, media, or capture integration whenever available. A desktop-backed app is a fallback bridge, not the default consumer architecture. Desktop-backed apps are allowed for development and unsupported services, but they are not considered complete smart-TV experiences until Isolde can present them full-screen, remote-first, and without host desktop chrome.

Games are a first-class living-room category. A game hub, such as Steam Big Picture or a future native library, should be presented as a controller-first console surface rather than as a generic desktop app. Individual games, detected game discs, emulators, and capture inputs should share the same remote/controller navigation expectations: full-screen, low-friction, and clear return-to-Isolde behavior.

Desktop Mode is not a third equal destination beside TV and games. It is an intentional, rarely used utility escape hatch for files, downloads, troubleshooting, modding, unsupported workflows, and other tasks that are not apps. It should be gated through a clear user action, labeled as advanced, and always provide an obvious path back to Isolde.

Isolde is an always-on, network-aware appliance shell, not an app that casually quits. Normal power behavior should keep the shell available, using ambient mode and display sleep rather than shutting down the experience. Shutdown belongs behind an explicit advanced path; restart and update/restart are separate appliance operations.

Audio is an appliance-level policy owned by Settings, not by individual tiles. Isolde should support the common living-room setup where the TV or soundbar is left at a stable mid-level reference volume and Isolde owns day-to-day volume, mute, night mode, and lip-sync behavior. The first implementation persists Isolde's intended volume owner, TV reference level, volume, mute state, startup volume behavior, remote step size, night mode, and lip-sync delay so the TV interface can model the experience end-to-end. A later mixer backend should attach these settings to PipeWire/PulseAudio, HDMI/CEC behavior, output-device selection, and per-session audio ownership without changing the user-facing contract.

Remote input is also an appliance-level boundary. Physical remotes, Bluetooth controllers, HDMI-CEC events, IR receivers, gamepads, and keyboards should all map into semantic Isolde commands rather than leaking device-specific key codes into QML. The first Remote Setup page persists a profile and setup/test state; later backends should add discovery, pairing, remapping, diagnostics, and profile export/import.

Isolde owns the internet connection and the service ecosystem when they are available, but the shell must not be internet-dependent. Streaming apps, the App Store, account sign-in, updates, remote support, smart-home dashboards, local-network discovery, and downloads are Isolde responsibilities. If internet is unavailable, Isolde should dim or disable internet-dependent apps such as Netflix, show clear retry/network-needed states, and keep local capabilities working wherever technically possible: Kodi, local files, discs, local media servers, LAN Home Assistant, capture inputs, installed offline games, Settings, and Desktop Mode.

Apps may declare `networkRequirement` as `none`, `local-network`, or `internet`. Home uses this state to present unavailable internet/cloud tiles differently from local capabilities. This is a presentation and safety contract, not a hardcoded service list: Netflix-like streaming packages should require `internet`; local media servers and LAN dashboards should require `local-network`; Kodi, discs, capture inputs, offline games, Settings, and Desktop Mode should usually require `none`.

Tile artwork is user-layout state, not app identity. Built-in art packs are loaded through a small catalog and expose safe symbols/colors that can be applied to app or action tiles. This lets Isolde ship a polished starter vocabulary for living-room platforms and devices while keeping official brand artwork, app-provided assets, and personal PNGs behind explicit asset boundaries.

Theme packs use the same catalog boundary and currently define background colors plus tile roundness. The shell previews theme data in QML without mutating `HomeLayout`; applying a theme calls the layout authority, which validates and persists the selected values atomically.

Home distinguishes permanent user-owned tiles from contextual detected-source tiles. Permanent tiles are saved layout items with typed action metadata, such as `launch-app`, `open-folder`, `open-file`, `open-url`, `open-disc`, or `desktop-mode`. Contextual tiles are proposed by source detection for newly inserted or unexpected discs/drives and should be removable, suppressible, or promotable into permanent layout tiles. This prevents an always-connected media appliance from becoming cluttered by every transient device it has ever seen.

Action execution is handler-based. A tile's action target is paired with `openWith`, which can name an installed app or a future system handler. Isolde must not hard-code Kodi, VLC, Steam, or any other single app as the universal media handler. The first implementation launches the selected app handler and rejects unimplemented system-default handling cleanly; later handler adapters will pass folder/file/disc targets to the chosen app when that app declares support.

Smart TVs connected to Isolde should be configured as simple displays where possible: last-input/power-on-input behavior, no required TV-vendor internet service, no required TV app ecosystem, and Isolde as the primary media center, app launcher, HDMI/input switch, game surface, and utility computer. TV-specific CEC/power/input support belongs behind display/settings backends, not inside individual app tiles.

Desktop-backed packages are launched through a constrained desktop-entry boundary. Isolde resolves the declared desktop ID to a readable `.desktop` file, validates that it is a non-terminal application, parses the Exec line without invoking a shell, resolves the executable path, and logs the command it actually starts. This preserves the App Store trust boundary: package manifests declare desktop-entry IDs and presentation metadata, not arbitrary shell commands.

The `web` session type renders HTTPS or packaged `qrc` TV surfaces inside Isolde's own WebEngine kiosk, not in an external browser. Web apps may declare `allowedOrigins`; navigation outside those declared origins is blocked and surfaced as an Isolde-owned retryable error, preserving the appliance feel.

## Session lifecycle

For 0.0.1.2, configured apps may use mock, store, settings, web, or desktop session implementations. A launch moves the session from Created to Running. Back or Home exits and destroys it, then emits the stable origin ID. The home view finds that ID in the current model and restores its selected index.

The `web` session type renders an HTTPS or resource URL inside an Isolde-owned `WebEngineView` with no browser chrome. This is the preferred path for services that expose a credible TV, living-room, PWA, or kiosk web surface. Capture-specific switching, signal acquisition, audio, and frame handling belong inside a future capture boundary—not QML tiles. Presentation policy belongs beside the session boundary so Isolde, not the host desktop, owns focus, fullscreen behavior, Back/Home handling, overlays, errors, and recovery.

Desktop containment is currently implemented as a Linux/X11 development bridge using declared window-class metadata plus `wmctrl`/`xprop` when available. Isolde retries containment while the app starts, records when the expected window is found, and returns Home if the launcher exits without producing the declared window. This is a hardening step for real-app experiments, not the final consumer isolation model.

## Configuration choice

JSON is used instead of YAML to avoid a prototype-only parser dependency; Qt provides JSON parsing directly. The build copies it beside the executable so definitions can change without recompilation. The same config is embedded as a dependable appliance-startup fallback.
