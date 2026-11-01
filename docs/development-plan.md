# Isolde development plan

> Current authority (2026-10-19 audit): [agent-backlog.md](agent-backlog.md) selects the next task and records completion; this document supplies requirements/dependency rationale. Older dated next-action and not-started statements below are historical. T02 curation extensions are out of scope; existing movie/show collections remain unchanged. Source presence is not acceptance. See [planning audit](planning-audit.md) and [VM oracle](vm-oracle.md).

Current development milestone: **0.0.1.2**

October 18 page-scoped universal search (Isolde Studio M4): the consolidated [unified master plan](unified-master-plan.txt) (v0.0.1.1) is now checked into `docs/` as the requirements reference that refines features already begun. Command Center universal search now integrates page-scoped sources: `PageModel::pageSearchSources()` exposes each page's library sections/widgets (`presentation.variant == "library"`) as `{id, name, kind, view, value}`; `CommandCenter.qml` surfaces matching page sources as searchable entries and emits `pageSourceRequested`; `Main.qml` populates page context through `openSearchFromPage()` (Home, PageSurface, `/` and `S`) and routes a selection back to the originating page. Search remains local-first with no external requests. Regression coverage added to `pagesPersistAndRouteAndTravelWithSkins` in the component suite. CMake/CTest and installed acceptance remain pending in the current Windows environment. The unified plan's Section 13 Web ON/OFF search toggle, title-detail and viewing-options flow remain future slices gated on a verified discovery backend.

October 17 universal grid foundation (Isolde Studio M1): `ComponentSchema` now validates an optional `composition.grid` map (columns 4–16 default 8, rows 3–10 default 5, gap, margin, snap, overlap) and per-widget `placement` rectangles (integer cells, overflow prevented, collision math shared by all callers). `HomeLayout` exposes `grid()`, `setGrid()`, `setItemPlacement()`, `clearItemPlacement()` and `gridCollisions()` for the future Studio editor; grid changes may not strand or newly overlap placed widgets, saves stay atomic with rollback, and stored documents with placements outside their grid fall back to packaged defaults without rewriting the file. Legacy v1 layouts load unchanged with default grid data and no migration write on read. This is the data contract only: no renderer consumes placements yet, and the grid editor/Studio shell remain future slices. Added C++ regression coverage (`universalGridValidatesAndPersists` in the component suite); CMake/CTest and installed acceptance remain pending in the current Windows environment. See [home composition](home-composition.md) for the field contract and [Studio builder](studio-builder.md) for the plan.

October 17 universal local search: Command Center now merges profile-local Kodi title results for Movies, Shows, and Episodes with saved collection names. Results use per-type bounded paging; Kodi-unavailable/searching states are explicit. Movie/episode selections launch through Kodi, while show selections revalidate into the selected show. Search is debounced, cancellation-aware, preserves the underlying Home/page/library, and makes no web/provider requests. Hardware-free bridge regressions were added; native execution and installed/live-Kodi acceptance remain pending because CMake/CTest are unavailable in the current Windows environment. Tag/playlist results and optional Web ON are still future scope. See [Kodi media](kodi-media.md).

October 17 manual curation: a Menu action on a movie or TV show's details creates a named profile-local manual collection or toggles that same-type item in an existing collection. IDs are hydrated through strict read-only Kodi movie/show detail bridge methods, up to 50 at a time, and can power Home shelves. No Kodi/media writes occur. C++ regression coverage covers both types, persistence, detail hydration, membership updates, shelf actions, and show navigation; native CTest and real-Kodi acceptance remain pending. Episode manual memberships and exclusions are still open.

October 17 Kodi settings theme consistency: Isolde-owned Media Library and Media folder access pages consume the active profile's saved appearance tokens. Native Kodi remains under its existing user skin; no Kodi skin setting is changed. An offscreen QML test covers themed selection surfaces in both pages; CMake/CTest execution is still pending in the current Windows environment.

October 17 capture foundation: Settings → System → HDMI Inputs now discovers USB video/audio devices and serial ports, configures a bounded RS-232 command template, names eight upstream switch inputs, and offers an Isolde-owned video preview. Default Home input actions stay unavailable until required hardware/protocol configuration is present. Capture-mode apps open the shared unassigned input picker, and Components links to setup. A full-screen session rechecks video-frame freshness after each switch and waits for visual user confirmation before labeling the route active. Installer dependencies now include Qt Multimedia/SerialPort and the QML multimedia module. Hardware-free protocol/schema tests are added. Build/CTest and physical card/switch acceptance are not complete in the current Windows environment; the capture card, model-specific switch protocol, protected-source handling, latency, resolution changes, hotplug and audio sync remain qualification gates. See [capture setup](capture.md).

October 16 Home composition: opt-in configured navigation, section lanes, banner/media/source/guide-summary cards, artwork crops, below-card titles and resume indicators now implement the reference's visual structure. A remote Component layout editor controls section style, density, height and order. Existing layouts still use the original grid until explicitly changed. Contract and actual QML tests cover persistence, validation, geometry, directional navigation and focus restoration. Installed recreation/reset captures and manual visual review are retained in [the reference test](../tests/visual/home-reference/README.md). The visual fixture is synthetic: live Kodi artwork/progress bindings, guide/capture qualification and full remote editing of every field remain separate work.

October 16 interface direction: the supplied design proposal defines a polished starting preset, not fixed tabs, category names or layout restrictions. [interface-design.md](interface-design.md) records the required customization matrix and revised sequence: shared appearance → editable navigation/composition → poster categories/shelves and curation → universal search. Make it yours adds draft preview/Apply/Cancel for per-profile colors, typography, Home density/spacing, corners and motion. Theme focus color and typography now persist into the real interface. The full proposed destination/shelf interface remains planned, not delivered by these appearance controls.

October 16 Kodi television slice: Movies/Shows navigation now includes seasons, Specials and episodes, direct resume/restart, Kodi-derived Recently Added/Continue Watching, local/cached artwork and parent-page/selection restoration. Kodi retains all library, progress and playback ownership. Thirteen automated suites pass, including real QML TV navigation and episode restart; live acceptance and deployment evidence is recorded in [kodi-media.md](kodi-media.md). Movie collections remain the existing scope. The next package is reusable collection/curation support through Kodi APIs, followed by universal local search; track selection and full customized-native-frontend acceptance remain open.

October 16 component foundation: shared versioned component/source/action validation now backs Home launchers, detected sources and generated collection shelves. Home v1 layouts load without writes, migrate on edit with an original backup, and preserve missing source bindings for remote repair. Central Home dispatch restores exact component origins. Permanent remote-operated Settings/Edit Home controls survive an empty layout. Twelve automated suites pass; installed release `45ca0a45d4f8e43f15e53b4d` passed key-driven repair/cancel, backup/migration, last-tile removal and empty-Home rebuilding in a disposable profile. Household was restored and visually verified, with zero service restarts. Failed layout edits retain the active profile state. See [components.md](components.md). Arbitrary nested composition/tabs remain the later editor slice; old application releases need the retained v1 backup for explicit layout downgrade recovery.

Current execution order: [implementation-order.md](implementation-order.md) reconciles the source with this historical log and the full master scope. Follow it instead of older “next action” paragraphs below. Shelf queries survive presentation edits and persisted collection rules are validated. Keep live collection-shelf playback/return in the next media acceptance matrix; the component UI acceptance above covers repair, migration and recovery.

## Master plan integration — October 15, 2026

First implementation deployed: per-user Isolde/Kodi frontend selection, idle-only clean-exit mode changes, shared native-home ownership locking and safe launch deferral; System → Components offline engine/version/element inspection; installer dependency declaration now includes Kodi and GStreamer. Nine test suites pass. Real Kodi round trip preserved custom maroon colors and sources/favourites/keymap/add-on-settings fixtures; native appearance and the installed Components page were visually verified. Full custom-skin/add-on/watch-state qualification, existing-desktop migration, live capture, guided repair, and offline dependency images are not complete. No capture hardware is present in the VM.

The attached 19-section Managed Capture master plan is incorporated in [master-roadmap.md](master-roadmap.md), which is the requirements and sequencing companion to this implementation log. It includes components/tabs/collections, dual Kodi frontends, strict local-only search, offline Linguist, direct local playback, the Generator Test, seamless HDMI sessions and managed GStreamer provisioning. These are planned requirements, not completed features. Preserve existing account isolation: the attachment's Everyday/Cinema/Family “profiles” are presentation presets. Keep version 0.0.1.2. Start with lossless Kodi mode foundations and managed-component provisioning/health; actual capture and offline installation-image qualification remain open.

## Active: existing-folder movie workflow and Isolde desktop

User-approved workflow: download a movie in a desktop browser, use the file manager to place it in the existing organized movie folder (or an existing subfolder), return Home, then Settings → System → Media Library → Scan for New Movies. Isolde must not copy, rename, move, flatten, automatically sort, or clean up this collection.

- Implemented October 15: remote-operable scan control, narrowly allowlisted Kodi scan command for configured video sources, asynchronous scan-state monitoring, duplicate-request guard, profile-stop cancellation, and library refresh afterward. Idle after request is explicitly not proof that new movies were found. This scans Kodi's configured video sources, not movies alone when TV sources are also configured. Eight suites passed; real Kodi accepted a scan in a separate empty test profile, and the installed UI scan button was exercised in Household. No real movie discovery is claimed.
- Implemented October 15: remembered per-profile read-only media-folder grants, remote-operated folder browsing/removal, and shared mounts for native/managed Kodi. Broad/system/private/symlink paths are refused; disconnected drives do not prevent startup. Changes require Kodi application mode with Kodi closed. Nine automated suites pass, including real sandbox isolation and write-denial checks. A real Kodi acceptance test configured a synthetic collection through Kodi's own UI, added a second movie in a nested folder afterward, and proved Scan for New Movies discovered both titles while all original media/NFO hashes stayed unchanged. First-time source/content configuration still uses Kodi's supported UI; integrated guided setup and playback remain open.
- Still required: an explicit Desktop entry for browser/file management; persistent Return to Isolde control and compositor-supported Home shortcut; Esc returns when the desktop/return control owns focus, without stealing application Escape; background/theme follows Isolde. Explain that this is a shared Linux maintenance workspace, not an isolated per-profile browser. Do not silently expose another profile's sign-ins.
- Preserve arbitrary existing directory organization. First-time setup chooses the collection root and movie content/recursive scanning; subsequent additions need only the scan button.

## Recent verification

October 16 local-library discovery slice: movie title search, Continue Watching sorted by last played, and reusable named title/tag/genre collections are implemented in the remote-operated Movies toolbar. Rules query Kodi's full library with bounded pages, not just loaded results; changing a query cancels superseded reads. Collections persist per Isolde profile with duplicate-name checks, rename and confirmed removal; Kodi metadata/media remain untouched. Playback stop refreshes the current filtered shelf. Ten suites pass before deployment. Real Kodi acceptance found a synthetic title, a positive tag/genre fixture and one saved-progress movie; nonexistent filters returned empty results. Remote-only keyboard entry saved a collection in a disposable profile. At that acceptance, collections were movie-only; universal cross-view/episode search, optional web search, manual title memberships, playlist/node adapters, shelf placement and exclusions remained open.

October 17 working-tree extension: saved title/tag/genre rule collections now target either Movies or TV shows, with legacy definitions defaulting to Movies. The Shows toolbar exposes collection save/open/rename/remove; show rules also produce Home shelves with membership-checked navigation into the selected show. Focused C++ regression coverage was added, but could not be executed in the current Windows environment because CMake is unavailable. Real-Kodi acceptance is still required.

October 16 playback slice: implemented local-library movie-ID play/resume and explicit restart, owned-player pause/seek/stop, bounded start monitoring, queued Home cancellation during startup, and natural-end/stop return to Isolde. Movies now play directly with OK; Menu opens details. Managed-only temporary keymaps preserve native Kodi keymap files. Live VM acceptance confirmed fullscreen synthetic video, remote OK pause, and Home stop (`paused` → `ended`, `home: true`). Ten suites pass before final deployment. The supported presentation path currently requires X11/XWayland, wmctrl and exactly one Kodi window; native Wayland/compositor-independent presentation, physical-remote hardware, audio/subtitle selection and real-world codec/audio qualification remain open. Do not equate silent synthetic video with speaker/audio acceptance.

October 15 library browsing slice: the Movies Home tile now routes to Isolde's real library (or native Kodi in Kodi application mode). Replaced the first-ten preview with title-sorted, bounded 50-item pages and total counts; left/right change pages, OK opens movie details, and Back preserves selection. Details expose synopsis, runtime, watched state and saved progress without modifying Kodi. Tests exercise 103 entries, last/previous pages, duplicate input, malformed pagination, empty libraries and profile invalidation. All nine suites passed. Playback remains the next lifecycle slice: managed Kodi is minimized, so simply issuing Player.Open is insufficient; visible video, transport controls and reliable Home return must be qualified together.

October 15 startup acceptance: `install --startup` now explicitly enables and verifies the user service; a bounded display-readiness gate fixes an early-Wayland startup race. Full installer/tests and a cold boot passed on the existing VM: Household Home appeared without manual launch and service restart count was zero. This closes the current VM's startup bug, not the broader clean-machine installer/autologin/compositor qualification work. The user also confirmed hearing the AC97 sound test in the room.

Sound test implemented in System → Audio: quiet left/right/both tones, current-volume/mute preservation, stop-on-exit and timeout cleanup. VM stall resolved October 15 by replacing virtual HDA with AC97/STAC9700; repeated real three-second playback probes complete and ALSA hardware pointers advance. Host-backend-only changes did not fix it. Keep microphone input disabled. Physical speaker audibility remains a user acceptance check; see audio documentation and `tools/audio-playback-check.py`.

Audio output selection: System → Audio now discovers WirePlumber playback outputs, previews choices with left/right, applies with Enter, and verifies the active default. Refresh and periodic discovery handle connected-output changes. Tests cover unplugging and rejected/unconfirmed selections; the actual VM selector was visually checked with its analog output. Existing-stream migration, PulseAudio-only selection, HDMI profiles, Bluetooth pairing, and multi-device physical acceptance remain open.

Audio hardening (October 2026): explicit settings namespace, asynchronous bounded mixer commands, confirmed volume/mute readback, external-level polling, startup policies applied once, and device-wide audio lifetime across profile changes. Unsupported CEC/Hybrid volume selection is disabled; Night Mode and Lip Sync remain unavailable. Eight test suites include a dedicated subprocess mixer suite. Real TV/receiver audibility and remote interception still require hardware acceptance; software mixer confirmation is not proof of audible output.

## PINNED — RETURN TO DEDICATED INSTALLATION BEFORE RELEASE

**Deferred by user request: do not lose this work.** Recovery is implemented, but dedicated boot-to-Isolde, autologin, compositor integration, clean-machine installation, reboot/power-loss tests, and signed USB/image distribution remain unfinished. Resume this track after the Kodi front-end milestone; it is a public-release blocker, not a completed feature. Guest administrator authentication is still needed for OS provisioning. The next active priority is local Kodi-backed browsing → playback → return inside Isolde.

## Dedicated-machine installation and onboarding — deferred

Core tenet: the target computer exists solely to run Isolde. Consumer installation must become download → explicitly select installation disk → install → short TV-first setup. Users must not assemble a Linux environment, configure isolation by hand, or depend on desktop access for normal operation.

- Implemented foundation: Ubuntu 26.04 dependency bootstrap/check, build-and-test-before-activation installer, separately staged content-addressed releases, offline application-release rollback, optional graphical-login startup with bounded crash restarts, and persisted remote-operated first-run setup. Existing profile and account data is preserved. Audio/Remote pages now scroll to selected controls.
- On-screen recovery implemented: an independent Qt Quick executable handles repeated application crashes, with remote-operated Retry and explicitly confirmed previous-release restoration. It avoids profile/catalog/Chromium initialization, shares the installer lock, and is pinned independently of the selected app release. OS/compositor failures and data migration recovery remain separate work.
- Current verification: five automated suites, actual VM staged installation and user service startup, deliberate crash-loop recovery, keyboard Retry, and confirmed rollback returning to Home. This does **not** establish blank-machine install, OS boot/autologin, authenticated service compatibility, or OS rollback readiness.
- Next blocking appliance slice: dedicated multi-app session and OS provisioning, explicit autologin policy, fresh VM install/reboot/power-loss acceptance, then signed packages and bootable USB/image delivery. Root access is needed for actual OS provisioning; do not silently disable host security controls or change the Windows host.
- Later onboarding completion: language/region, privacy/update choices, hardware-specific remote/CEC verification, and first-app recommendations. Existing settings reuse is a functional starting point, not the final polished consumer setup design.
- Instructions and limitations: [Installation](install.md).

## Planned media bridge: Kodi-backed entertainment

Accepted direction from the October 2026 Kodi Integration Plan: Isolde owns browsing, navigation, widgets, and system controls; Kodi can supply media libraries, playback, artwork, watched/resume state, PVR, and add-ons. This is the active development priority. Kodi and the media files are expected to live on the same dedicated computer; no separate media server is required. Keep Kodi running locally between videos within the active profile. The primary acceptance criterion is a usable Isolde front end, not backend infrastructure alone. Keep the current milestone at **0.0.1.2**; these phases do not imply separate version releases.

Architecture: QML screens/widgets → C++/Qt media-provider interface → Kodi adapter (JSON-RPC, cache, artwork, playback/lifecycle coordination). Do not adopt the attachment's illustrative TypeScript stack. Keep other providers possible and preserve user-controlled tile actions and compatible “Open with” choices; do not make every media action implicitly launch Kodi.

First implementation slice: read-only Kodi service add-on with a private filesystem socket (no HTTP/LAN listener), profile-bound asynchronous Qt client, and Ctrl+M library preview in Isolde. Live Kodi version/API/library reads verified in a separate test profile; full browse/play/return acceptance remains open. See [Kodi media bridge](kodi-media.md) for setup, tests, and explicit limitations.

Managed startup implemented: installed Kodi starts behind Isolde after setup, using a disposable bundled-add-on overlay and the active profile's isolated home. System → Kodi startup provides per-profile enable/disable, status, and retry. Fresh and repeat startup passed against real Kodi without manual add-on activation; seven automated suites pass. OS boot/autologin remains pinned separately. Settings now label unimplemented night compression, lip-sync delay, reduced motion, and OS power controls as unavailable rather than pretending they work.

1. **Connection and isolation:** inspect the installed Kodi/API versions; provision an authenticated local control endpoint without exposing credentials to QML/browser content; verify actual listener binding and reject unintended LAN exposure. Add bounded timeouts/reconnects and clear diagnostics. Keep service credentials, cached metadata, artwork, and watch state scoped to the active profile. Shared network namespaces mean localhost alone does not establish profile isolation.
2. **End-to-end playback proof:** retrieve ten movies and artwork, browse/select in Isolde, launch full-screen Kodi playback, and restore the same Isolde screen/selection afterward. Verify pause/seek/volume, resume positions, watched state, disconnects, Kodi crashes, and profile switching. Distinguish Back (dismiss overlay/navigation) from explicit stop/return. Do not claim JSON-RPC embeds Kodi's renderer.
3. **Library and widgets:** paginated movies/shows/episodes, metadata/artwork caching, Continue Watching and Recently Added; responsive offline/cached states with stale data identified. Widgets call the bridge, not Kodi directly.
4. **Live TV/PseudoTV:** inspect the actual installed add-on/backend first. Prefer supported PVR channel/EPG APIs when available; otherwise evaluate a playlist/guide adapter. Preserve channel definitions and schedules. Add On Air Now, a remote-navigable guide, channel switching, and restart/schedule synchronization tests.
5. **Reliability and advanced access:** manage Kodi between videos within the active profile, not as an unmanaged always-shared process. Stop and clean up the outgoing profile's service before starting the next; discard in-flight replies and profile-specific caches. Retain Open Kodi for advanced settings/add-ons. Coordinate focus, sleep/wake, crashes, and display changes without disabling Isolde when Kodi fails.

Engineering constraints: use supported APIs rather than direct database edits; avoid forking Kodi; leave existing libraries/add-ons intact. Persistent library access needs explicit, remembered media grants compatible with the native sandbox—not unrestricted access to the host home. Validate service binding/authentication, native window transitions, real media playback, and PseudoTV compatibility before marking the corresponding phases complete.

## Personal profiles and app isolation

Core requirement: each person has independent app instances, sign-ins, and personal presentation. Switching profiles must never expose the previous person's web session. This is a storage and lifecycle requirement, not merely a Home-layout preset.

- Implemented in 0.0.1.2: named profiles, remote-operated creation/rename/switching, remembered active profile, separate Home layouts and ambient preferences, and persistent Chromium identities with full interface teardown on switching. Existing layout is retained as Household.
- Verification: layout/settings regression tests and actual Chromium cookie/localStorage/IndexedDB isolation and persistence tests. Real YouTube account sign-in requires manual acceptance with user accounts.
- Native isolation implemented: Linux native apps now use per-profile/per-app sandboxed homes, private session buses, private temporary/process namespaces, explicit read-only media grants, and graceful shutdown with forced descendant cleanup. Missing dependencies and unsupported delegated launchers fail closed. The Store identifies private native app data. Kodi and Spotify have been visually launched with this runtime; signed-in account switching is still a manual acceptance test.
- Still required: Steam and other native-app compatibility validation, dedicated Snap/Flatpak and external-login integrations, optional PINs and startup chooser, guest mode, parental controls, and deletion/account-reset flows. Shared display/audio/network access means this is not an adversarial OS-user security boundary.
- Detailed storage boundaries and acceptance steps: [Profiles](profiles.md).

This document extends the original 0.0.1.1 architecture handoff. The core product contract remains: everything is an app, every source is represented by a tile, and normal use is remote-first.

## Appliance thesis

Isolde should behave like an always-on, network-aware living-room appliance. The expected installation is a small computer or VM-backed host connected to a TV input, with the TV configured to return to that input on power-up. Isolde should own and use the internet connection when available, but it must not stop functioning when internet is unavailable. The smart TV itself should not need its own internet connection, apps, accounts, ads, or vendor home screen because Isolde is the smart-TV brain, media center, HDMI/input switch, app surface, game surface, and utility computer.

This has product consequences:

- Isolde should not “turn off” during normal use. Power controls should favor display sleep, ambient mode, restart, update/restart, and explicit shutdown only behind an advanced confirmation.
- Isolde should own the internet connection, app accounts, streaming services, smart-home dashboards, downloads, updates, and remote access when those services are available.
- The TV attached to Isolde should be configurable as a simple display: last input on power-up, no required TV-vendor network, no required TV app ecosystem, and no reason to visit the TV's own home screen during normal use.
- The TV or soundbar volume should be set once to a comfortable mid-level reference. Isolde should then own everyday volume, mute, night mode, and lip-sync behavior wherever the audio backend allows it.
- Plex, Home Assistant, Kodi, streaming apps, local files, discs, capture inputs, game hubs, and utility desktop access should be easy to add from the Isolde TV interface.
- Local-network services should support discovery and manual address entry, then persist friendly tiles/actions.
- Internet failures should be handled by Isolde with clear recovery states. Internet-dependent tiles, such as Netflix or cloud streaming services, should be dimmed/disabled or show a retry/sign-in/network-needed state. Local capabilities, such as Kodi, local files, discs, local Plex/Jellyfin servers, Home Assistant on the LAN, capture inputs, installed offline games, Settings, and Desktop Mode, should continue working wherever technically possible.
- Apps and packages should declare whether they need no network, local network, or full internet so Isolde can make these choices consistently instead of relying on hardcoded service names.

## Product quality bar

Isolde is intended to become a comprehensive, professional-quality product. Prototype status may limit feature breadth, but it does not excuse fragile foundations, ambiguous interaction, unsafe state changes, or undocumented behavior.

Every milestone should be held to the following standards in proportion to its scope:

- **Architecture:** clear ownership, explicit state, stable interfaces, replaceable implementations, and separation between presentation, user data, installed capabilities, and session lifecycle.
- **User experience:** consistent remote-first behavior, predictable focus, polished empty/loading/error states, safe confirmation patterns, undo or recovery where appropriate, and no accidental exposure of the underlying desktop.
- **Accessibility:** readable 10-foot typography, strong focus visibility, usable contrast, scalable presentation, reduced-motion support, and future accommodation for screen readers and alternative input.
- **Reliability:** atomic persistence, schema validation, safe defaults, corruption recovery, idempotent operations, meaningful logs, and graceful degradation.
- **Security and privacy:** least privilege, declared capabilities, trusted update paths, explicit permission boundaries, no advertising-driven Home behavior, and no collection of user data without informed consent.
- **Testing:** unit, integration, UI, migration, failure-path, and repeatable manual acceptance coverage, with deterministic fixtures and captured evidence for important user journeys.
- **Performance:** responsive input, bounded startup time, stable frame pacing, controlled memory/resource use, and later measurable latency budgets for capture and audio.
- **Maintainability:** warning-clean builds, consistent formatting, focused components, reviewable changes, useful diagnostics, and documentation that matches implemented behavior.
- **Compatibility:** versioned schemas, forward migration, safe rollback, declared platform requirements, and deliberate handling of unsupported configurations.
- **Release discipline:** meaningful milestone versions, reproducible builds, release notes, acceptance criteria, known limitations, and a recovery path before distributing a build.

Temporary shortcuts must be labeled, scoped, and tracked. Mock implementations should conform to the same contracts expected of later production implementations so replacing them does not require redesigning the shell.

## Versioning policy

Development versions represent meaningful, consolidated milestones—not individual edits, builds, screenshots, or test passes. Related work accumulates under 0.0.1.2 until its milestone is coherent and demonstrable. Patch identifiers should not be advanced merely because another internal build was produced.

Public releases will use deliberate release criteria and release notes. Internal build identity, when needed, should come from source-control revision or build metadata rather than consuming product version numbers.

## Product principle: user-owned Home

Isolde should be highly customizable. The default experience must remain calm and usable, but the user—not Isolde, advertisers, or an app provider—owns the Home screen.

The customization system should eventually allow users to:

- move and arrange tiles with directional controls;
- organize tiles into rows, sections, folders, or pages;
- choose tile sizes from a small TV-appropriate set;
- set a background using a color, built-in design, or personal image;
- apply built-in theme/art packs for common living-room platforms, app categories, source types, and gaming devices;
- hide or remove tiles without uninstalling their underlying integration;
- restore a sensible default layout;
- create a tile and link it to an available action;
- edit a tile's name, visual identity, action, and placement;
- export, back up, and restore the Home layout.

Customization must remain fully usable without a mouse. Edit mode must be visually unmistakable and provide safe cancel/undo behavior so ordinary navigation cannot accidentally rearrange Home.

Isolde ships with a complete default Home layout. Customization is optional: a new installation, a test profile, or a user who never opens the Home editor receives a polished and fully functional arrangement immediately.

Built-in theme/art packs should provide a professional starter vocabulary for common living-room concepts: streaming, music, media centers, smart-home dashboards, consoles, game hubs, movie discs, game discs, folders, capture inputs, and utility desktop access. These packs should be data-driven and replaceable. Isolde-owned starter badges are acceptable for development and default presentation; official brand artwork should be handled later through licensed packs, app-provided assets, or user-selected PNGs rather than copied into the shell casually. Applying a theme should always use a preview-first flow: browse themes, inspect a TV-scale sample Home surface, then explicitly apply or cancel without mutating the saved layout.

Theme feature stopping point for now: Isolde has a catalog-backed starter pack, built-in art selection, theme preview, theme-only apply, theme-plus-suggested-art apply, validated theme metadata for background, focus color, typography scale, tile roundness, tile opacity, and suggested art mappings. Follow-on theme work should wait until larger product systems expose new styling needs; do not spend the next slice polishing theme minutiae.

## Settings and Home editor

Status in 0.0.1.2: Settings is becoming the place where a user can operate the system without leaving the TV view. The remote-first Home editor supports persistent ordering, removal, compact/standard/wide sizing, background palettes, continuous corner roundness, optional Home branding, independent logo/title/type visibility, managed PNG artwork, arbitrary user-created categories, clock widgets, and recovery to the tested default layout. Network Settings now begins the system-control layer with connection status, device status, connection details, Wi-Fi scan, refresh, Wi-Fi radio control, scanned-network join, hidden-network join/password flow, and saved-profile reconnect/forget actions through NetworkManager. Settings now treats Remotes as a first-class section beside Network, Home, Sources, and System; the first Remotes page exposes setup mode, profile selection, live semantic button testing, power-button behavior, HDMI-CEC status, and TV on/standby test actions. System Settings retains the TV-control hierarchy for Display, Audio, Ambient Mode, Power, and About. Display Settings now includes persisted TV safe-area, UI scale, reduced-motion controls, a live calibration preview, and a global shell viewport that applies safe area and scale across Home, Settings, Store, and Isolde-owned session views. Audio Settings now exposes persisted living-room volume owner, TV reference level, volume, mute, startup volume policy, remote volume step, night mode, and lip-sync delay as an Isolde-owned policy surface; volume up/down/mute are global semantic commands and the Linux mixer is attempted through `pactl` or `wpctl` when available. Remote power can invoke safe `cec-client` TV on/standby commands when HDMI-CEC is available, with Ambient Mode fallback when unavailable. Bluetooth pairing, evdev/libinput device identity, CEC event monitoring, output-device selection, and remote remapping remain follow-on backend work. Smart TV app presentation now includes explicit `native-tv`, `web-kiosk`, `media`, `capture`, and `desktop-bridge` modes, plus an Isolde-owned `web` session for HTTPS kiosk surfaces such as YouTube's TV web entry point, loading/error overlays, and declared allowed-origin navigation. Weather widgets, widget settings, profiles, full display/audio backends, accounts, updates, and undo remain follow-on work.

A future Settings area will include a Home editor. The likely remote-first workflow is:

1. Open Settings, then Customize Home.
2. Select a tile or choose Create Tile.
3. Move, resize, rename, restyle, hide, or change its action.
4. Preview the result.
5. Save or cancel the edit session.

Layout data must be separate from app definitions. An installed app describes what it can launch; the user's Home layout describes whether, where, and how it appears. Stable IDs link the two.

Permanent or expected sources belong to the user-owned layout, not the automatic-detection layer. A NAS folder, always-attached media drive, favorite app action, desktop utility target, local service dashboard, or recurring game/library source should be created through an Add Tile → Assign Action workflow. Newly inserted or unexpected discs/drives may appear automatically as contextual tiles by default, but the user must be able to disable automatic appearance, hide a source, or promote a detected drive/source into a permanent tile.

Settings should eventually cover every appliance-level task that would otherwise force the user out to the host desktop: network, Bluetooth/remotes, display, audio, storage, accounts, privacy, updates, accessibility, power, screenshots/remote support, and recovery.

Install and first-run setup are product features, not afterthoughts. The preferred consumer path is an appliance image or signed package that boots into Isolde, starts the first-run wizard, configures display/audio/remote/network, and avoids requiring the user to understand Linux desktop administration. Developer and VM setup should remain documented separately as a safe test path.

Settings should also serve as the stable product-identity surface. The Isolde name and the installed product version should always be visible in Settings, regardless of whether Home branding is enabled. Build or source-control metadata may appear in an About/details view when useful, but it must remain distinct from the product version.

Desktop Mode should exist as a rare utility escape hatch, not as an equal product mode. Its purpose is to reach things that are not apps: files, downloaded installers, advanced troubleshooting, modding tools, unsupported setup flows, or a Windows/remote-desktop environment if that becomes the best implementation. Normal entertainment use should return to TV mode or Game mode as quickly as possible.

## Desirable extension roadmap

These extensions are desirable product directions beyond the current 0.0.1.2 slice. They should be designed as professional, user-facing systems rather than one-off screens:

1. **TV-native account system:** user profiles with separate Home layouts, installed apps, preferences, accessibility choices, parental controls, watch/search history boundaries, and guest/recovery behavior.
2. **On-screen keyboard:** a polished shared keyboard for Wi-Fi, search, Store browsing, account login, and supported app/web sessions.
3. **Widget framework:** addable and configurable widgets for time, weather, calendar, music playback, smart-home status, HDMI preview, reminders, photos, and system health.
4. **App permissions system:** TV-readable prompts and settings for network, location, microphone, storage, accounts, notifications, background playback, and device-control privileges.
5. **App Store package updates:** versioned updates, rollback, release notes, trusted publishers, signatures, compatibility checks, progress UI, and failed-install recovery.
6. **Universal search:** search across installed apps, Store listings, Settings, media sources, web-kiosk apps, and later local/network media libraries.
7. **Remote-control pairing and customization:** Bluetooth/remotes/controllers, button remapping, long-press actions, input-lag calibration, accessibility shortcuts, and troubleshooting.
8. **Smart display and audio settings:** resolution, refresh rate, HDR, overscan/safe area, audio output, volume behavior, HDMI-CEC, night mode, captions, and audio-delay controls.
9. **Screen saver / ambient mode:** clock, art, photos, slideshow mode, TV-native source manager, managed photo folders, sample import, advanced folder-path entry, shuffle/order, fit/fill/stretch, weather, now-playing visuals, low-power behavior, user-controlled timeout, configurable slideshow rate, and customizable idle transitions.
10. **Capture/input switching system:** HDMI capture devices, consoles, PCs, streaming boxes, live previews, custom names/icons, audio policy, and low-latency input switching.
11. **Living-room game platform:** Steam Big Picture or similar game-hub launching, controller-first game sessions, per-game tiles, game-disc detection, emulator/library integrations, save/state handoff policies, low-latency display/audio behavior, and clear Home return behavior.
12. **Utility desktop escape hatch:** an intentionally advanced Desktop Mode for rare non-app workflows, with optional Linux desktop, Windows VM, or remote-desktop backend research, confirmation/gating, and a prominent return-to-Isolde path.
13. **Always-on resilient appliance behavior:** display sleep instead of app shutdown, robust ambient mode, Isolde-owned internet/network settings, graceful offline state, App Store connectivity when online, local-network service discovery, TV last-input guidance, update/restart separation, and clear recovery if a launched app or service fails.
14. **Local service onboarding:** Plex, Home Assistant, Jellyfin, NAS shares, SMB/NFS media folders, DLNA/UPnP sources, and other household services should be addable with discovery, manual URL entry, credential handling, connection tests, and editable tiles.

15. **Friendly install and first-run setup:** appliance image, signed Linux package, VM/developer setup, autostart/kiosk integration, first-run wizard, recovery/rollback, update channel choice, and TV setup guidance.

## Input and on-screen keyboard

Add a remote-first on-screen keyboard so users can type inside Isolde apps and supported launched-app sessions without a physical keyboard. It should be treated as a shared system input service rather than reimplemented separately by each app.

The keyboard design should:

- open predictably when an editable field receives focus and be manually dismissible;
- support D-pad, confirm, Back, and common remote-control input behavior;
- preserve the user's place and restore focus correctly when it closes;
- provide shift, symbols, cursor movement, delete, space, and submit actions;
- avoid obscuring the active field, with a clear indication of where text will be entered;
- handle passwords and other sensitive fields without leaking their contents;
- define an integration path for supported native, web, and captured app sessions;
- include accessibility, localization, alternate layouts, and hardware-keyboard coexistence in its architecture.

The initial implementation targets Isolde-owned text fields first, beginning with Network Settings and Store search. Typing into arbitrary third-party apps should remain gated on a reliable, secure text-injection and focus bridge.

## Customize Home design review

Organization and a believable, coherent TV experience in Customize Home are active product concerns. Before expanding the editor further, review its information architecture and visual presentation as a complete user journey rather than only as a collection of controls.

The review should verify that:

- related controls are grouped into clear, understandable sections;
- common actions are easy to find while advanced styling controls do not dominate the screen;
- the selected item, current operation, edit mode, preview, save/cancel state, and consequences of removal are always unambiguous;
- categories, tiles, widgets, and global Home appearance are presented as distinct concepts;
- the preview accurately represents the real Home layout, including spacing, typography, artwork, focus, and safe-area behavior;
- labels and control behavior feel like a finished living-room product rather than an internal configuration panel;
- the experience remains usable and readable at TV distance with only a remote.

This review should produce a revised screen hierarchy and focused acceptance tests before additional customization breadth is prioritized.

Layout resolution follows this order:

1. Load the versioned default layout shipped with Isolde.
2. Apply a valid user-layout override when one exists.
3. Fall back safely to the default if the user layout is missing, malformed, or incompatible.

Reset to Defaults removes or archives the user override and restores the shipped layout. It must not uninstall apps or erase unrelated settings.

Suggested future data boundaries:

- `AppDefinition`: installed capability and launch metadata;
- `TileDefinition`: user-facing name, artwork, size, and linked action;
- `HomeLayout`: ordered sections/pages and tile placement;
- `ActionDefinition`: typed action exposed by the shell or an installed app;
- `ThemeDefinition`: background and presentation preferences.

## Create Tile workflow

Create Tile should let a user build a launcher without writing configuration files. Candidate action types include:

- launch an installed Isolde app;
- open a supported web destination;
- launch an approved native application;
- select a named capture input;
- open a local media destination;
- run a safe shell-provided action such as Settings, Search, or a playlist;
- later, invoke an automation or multi-step action.

Arbitrary command execution must not be the default consumer workflow. Actions should use typed, validated definitions with clear permission and trust boundaries. Advanced actions can be supported later behind explicit warnings and review.

The intended user flow is Add Tile → Assign Action → Configure Target → Open With → Choose Appearance → Place Tile. The first action types are `launch-app`, `open-folder`, `open-file`, `open-url`, `open-disc`, and `desktop-mode`. `Open With` must be data-driven by compatible installed apps and system handlers rather than hard-coded to Kodi or any other single application. Apps declare `handledActions`, and the Add Tile flow only offers handlers that can handle the selected action type. This model is also how detected discs or drives become permanent: the detection system proposes a contextual tile, and the user may choose “Keep on Home” to save it as a normal action tile with a chosen handler.

Current implementation status: permanent action tiles can be created from Customize Home, store a typed target and `openWith` handler, and launch through the chosen installed app handler. The Add Tile UI only offers installed apps that declare compatibility with the selected action type; `System default` remains a backend concept but is no longer presented as a user-facing choice until a safe system-handler exists. Desktop-backed handlers now use package-declared `actionHandlers` to pass the selected target into the launched app, either by appending the target or by using explicit argument templates. Kodi proves this without hard-coding Kodi-specific behavior into Isolde. A first Source Manager now projects compatible detected folders/files as contextual Home tiles, keeps them separate from the user's permanent layout, supports hide/disable/permanent state, and can promote a detected source into a normal action tile. Follow-on work should add true optical-disc/game-disc classification, richer target pickers, system handlers, Settings UI for source policy, and per-action acceptance tests against real media paths/devices.

## Source manager

The source manager owns temporary, contextual sources that should appear automatically but should not silently become part of the user's permanent Home layout. Its first implementation scans known removable/media roots, filters out sources with no compatible installed handler, classifies detected folders/files as `media-folder`, `file-source`, `movie-disc`, or `game-disc`, and exposes them as `Detected Sources` tiles. These tiles are ordinary typed actions at launch time, but they carry `contextual: true` and source metadata so Home and Settings can label them, keep them separate, and offer source-specific commands.

Initial source commands:

- Enter opens the detected source with the first compatible installed handler.
- Keep on Home promotes the source into a normal persistent action tile and marks the detected source as permanent so it no longer appears twice.
- Hide source suppresses that detected source.
- Automatic contextual source tiles can be disabled in backend state; the full Settings UI for this policy remains follow-on work.
- Settings now has a first-class Sources section with automatic-tile policy, refresh, visible-source count, source classification, handler display, and hide-selected behavior.

The next professional slice should replace simple path scanning with a richer device model: removable drive identity, optical media presence, movie-vs-game classification, friendly labels, configured permanent drives, ignored devices, and handler ranking by action type.

## App Store

Isolde should provide an App Store that feels native to the TV shell. It should support discovering, installing, updating, disabling, and uninstalling apps or integrations with directional input.

The App Store is a later milestone, but its requirements affect architecture now:

- packages need stable IDs, versions, compatibility metadata, and declared capabilities;
- installation must not silently gain unrestricted host access;
- package provenance, signatures, and update channels must be visible and verifiable;
- apps must declare their actions, configuration needs, artwork, and session type;
- installation and Home placement must be separate choices;
- sideloading, if supported, must be clearly distinguished from trusted-store installs;
- rollback or recovery must exist for failed installs and updates;
- store content must not turn Home into an advertising surface.

The first App Store prototype should use a small curated local catalog and mock installation state before any remote marketplace or package execution is introduced. Real desktop apps may be explored only through validated desktop-entry launchers after the OS package is separately provisioned in the trusted VM.

Current hardening status: the App Store can now expose Kodi as a non-standard media package, and the desktop-app bridge resolves the declared `.desktop` file itself rather than blindly delegating to `gtk-launch`. The bridge validates that the desktop entry is an application, rejects terminal entries and shell-like Exec syntax, resolves the actual executable, starts it directly with diagnostics, and retries focus/fullscreen containment from the declared window class. Desktop packages that claim file/folder/disc action support must declare an action adapter, so source tiles can pass their target into the app without hard-coded product names. Desktop sessions that fail after launch now unwind back to Home instead of leaving an inert placeholder session.

Plex and Home Assistant are now represented as curated web-kiosk package prototypes. Plex proves the “add a household media service” path; Home Assistant proves the “add a local smart-home dashboard” path. The prototype Home Assistant package uses the public demo URL only so the package can be tested without a local server; the finished flow must discover or ask for the user's actual local Home Assistant URL.

## Smart TV app presentation

Isolde may use Linux desktop technologies underneath, but the user experience must not feel like a desktop computer. A launched app should feel like a smart TV app or full-screen media surface, not a resizable Ubuntu window with exposed panels, title bars, dialogs, or desktop chrome.

Core philosophy: whenever an app has an official TV, living-room, remote-first, PWA, or web-kiosk experience, Isolde should prefer that version over a general desktop client. Desktop clients are fallbacks for development, unsupported services, or advanced users—not the default path for a consumer smart-TV appliance.

The shell should treat app presentation as a first-class contract:

- app selection and Store ranking should prefer `native-tv`, then `web-kiosk` or purpose-built media/capture integrations, then contained desktop fallback only when no credible TV-first option exists;
- web-kiosk apps should render inside Isolde's own browser surface rather than launching an external desktop browser;
- apps launch full-screen, borderless, and focused whenever the underlying platform allows it;
- Isolde retains a consistent Back/Home/overlay behavior above every app type;
- desktop panels, docks, crash dialogs, file managers, package prompts, and keyring prompts must not appear during normal appliance use;
- app launchers should declare whether they support native TV mode, web/kiosk mode, desktop containment, media mode, or capture mode;
- desktop containment declarations should include enough metadata to focus and fullscreen the app window, such as a validated window class and explicit containment policy;
- unsupported desktop-only behavior should be labeled as a development bridge, not a finished consumer app experience;
- app focus, quit, suspend/resume, audio ownership, and error recovery should be controlled by Isolde rather than by the host desktop;
- long-term, Chromium/web apps should prefer kiosk/PWA-style presentation with remote-first navigation and safe permission prompts.

Games extend the same presentation contract. Steam or another game launcher should prefer a living-room mode such as Steam Big Picture, not a normal desktop client surface. Isolde should distinguish a game hub from an individual game, a detected game disc, and a captured console/PC input so each can receive the right focus, controller, fullscreen, audio, and return-home policy.

Desktop Mode is deliberately different from `desktop-bridge`. A `desktop-bridge` app is an app fallback that Isolde tries to contain. `utility-desktop` is a user-invoked advanced environment for things that are not apps. It should be visually and behaviorally separated from normal Home use so it does not feel like Isolde accidentally leaked the host computer.

For the Spotify experiment specifically, the Debian desktop client proves that Isolde can install and launch a real external app, but it is not the preferred product architecture. The preferred Spotify direction is an official TV/web/kiosk-style integration if one is viable; the Debian desktop client remains a contained fallback until that route is proven or rejected.

## Revised milestone direction

### 0.0.1.2 — Shell hardening and customization foundations

- complete warning-clean Qt/CMake configuration;
- add automated QML navigation and lifecycle acceptance tests;
- add graceful configuration-error handling;
- separate installed app metadata from persisted Home layout data;
- define tile size, ordering, background, and action schemas;
- ship a deterministic default layout and theme for new users and tests;
- prototype a remote-first Home edit mode with move and resize;
- keep all existing mock sessions and focus-restoration behavior working.
- establish the App Store lifecycle first with a curated local catalog, persisted mock installs, and remote-first install/uninstall UI;
- prototype a Chromium-rendered Store surface backed by native Qt package lifecycle APIs;
- validate the first real desktop-app bridge with Spotify through a constrained desktop-entry manifest;
- validate non-standard living-room applications with Kodi as a media-center package from the trusted VM package repository;
- harden desktop package launching by resolving validated desktop entries directly, surfacing failures, returning Home on failed external launches, and proving fullscreen containment with Kodi;
- harden remote test ergonomics by eliminating lock/keyring prompts that block unattended screenshots;
- replace the direct customization app with a Settings hub and add the first Network Settings slice;
- begin the shared on-screen keyboard with Isolde-owned Wi-Fi text entry;
- establish the always-on resilient appliance principle, including Isolde-owned internet/network operation, graceful offline behavior, TV last-input guidance, and advanced-only shutdown;
- add app/package `networkRequirement` metadata and Home tile offline-state presentation so internet-only tiles can dim while local capabilities remain available;
- add Plex and Home Assistant to the curated package catalog as easy-install web-kiosk service prototypes;

### 0.0.2.x — Actions and real session experiments

- introduce typed `ActionDefinition` and `TileDefinition` models;
- add Create Tile and Edit Tile workflows;
- persist and restore user layout/theme choices;
- experiment with approved native, web, kiosk, media, and captured session launching;
- add game-hub, game, disc-media, disc-game, and utility-desktop presentation policies;
- prototype detected disc/device tiles for newly inserted movie and game media with configurable visual treatment, default actions, automatic appearance on/off, hide-source behavior, and “keep as permanent tile” promotion;
- add a Steam Big Picture package experiment as the first game-hub acceptance case;
- add local-service onboarding for Plex/Home Assistant style apps, including discovery/manual URL entry, connection test, credentials/account notes, and tile creation;
- add appliance power policy in Settings: ambient mode/display sleep by default, restart/update controls, and advanced confirmation before shutdown;
- add a smart-TV app presentation policy so desktop-backed apps cannot silently expose host chrome as the consumer experience;
- add safe action validation and permission presentation.
- revise Customize Home information architecture and visual hierarchy based on a full remote-first design review;
- add persistent Isolde name and product-version identification to Settings;
- define the shared on-screen keyboard service and prototype it for Isolde-owned text fields when development capacity permits.
- extend the shared on-screen keyboard beyond Network Settings into Search, Store, Settings forms, account login, and supported web-kiosk forms;

### 0.0.3.x — Curated App Store prototype

- browse a small local catalog;
- simulate install, update, disable, and uninstall lifecycle;
- declare actions and capabilities through package metadata;
- add trust, signature, compatibility, rollback, and permission models before remote installation is enabled.

HDMI capture, switching, media integration, audio policy, overlays, remote hardware, and appliance-mode work remain on the longer-term roadmap established by the original handoff.

## Immediate next development action

For 0.0.1.2, the next major product step is App Store/package hardening rather than more visual customization. Command Center, Settings, App Store browsing, Home customization, web-kiosk sessions, ambient mode, and real Kodi launch are now proven enough to expose the next risk: packages need a professional install/onboarding/recovery story before Isolde should feel like a real appliance.

The next package-hardening slice should:

1. Add a TV-native first-run/onboarding surface for packages that need setup, using Kodi's media-source prompt as the first acceptance case.
2. Surface launch failures in an Isolde-owned error page with retry, app details, uninstall, and diagnostics actions.
3. Distinguish installed-but-not-ready, ready, failed, and needs-setup package states in Store, Home, and Command Center.
4. Add package capability/permission presentation before launch for desktop-backed and media packages.
5. Add deterministic launch smoke tests through the `--launch-app=<id>` harness for Kodi, Spotify, and web-kiosk apps.
6. Record manual evidence screenshots and logs for each real package path.

After this, deepen web-kiosk sessions with TV navigation affordances: focus rings for common web controls, pairing-code/login guidance, per-app user-agent or URL variants, and tests around blocked navigation/error recovery.

Home skins and structured design editing are implemented; see [skins.md](skins.md). Skins preserve Home appearance and layout with embedded artwork, while Kodi remains responsible for media state and playback.

Reusable destination pages now share the Home design/component system: page management, own palette/layout/artwork, left/top/standalone navigation, Home/button/tab routes, Movies/Shows overrides, independent Kodi widgets, text/buttons and complete interface skins. See [pages.md](pages.md).
