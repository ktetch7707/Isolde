# Isolde master roadmap

Additive shell-surface requirements (user, 2026-10-19): [shell-surfaces-design.md](shell-surfaces-design.md) governs configurable contained source notices (T25), a dedicated All Apps gallery distinct from Search (T26), shared Home/page theme inheritance through Search/Settings/Isolde Kodi screens (T27), and a genuine default-top-right Search header switch (T28). Priority revision (user): finish the complete core Studio T15-T24 first. T22 depends on T21; the four shell fixes no longer block templates, Preview/Apply or core cutover. Then T27 depends on T24; T25/T26 consume T27; T28 depends on T25/T26/T27 and verifies their combined integration with the accepted Studio. This adds four zero-percent tasks: 24 active tasks, 11% overall. No application or VM change is part of this planning revision.

October 19 Page Studio amendment: the user rejects the existing Customize Home interaction. [Page Studio redesign](page-studio-redesign.md) now governs the replacement page editor; T15-T24 replace T06-T08 and have explicit priority in the backlog. Reuse native rendering and existing data ownership while replacing fragmented immediate-save tools with a canvas-first draft workspace. This amendment does not reopen T02 curation, qualify hardware, or authorize implementation during the planning-only request.

> Current authority (2026-10-19 audit): [agent-backlog.md](agent-backlog.md) selects the next task and records completion; this document supplies requirements/dependency rationale. Older dated next-action and not-started statements below are historical. T02 curation extensions are out of scope; existing movie/show collections remain unchanged. Source presence is not acceptance. See [planning audit](planning-audit.md) and [VM oracle](vm-oracle.md).

See [implementation-order.md](implementation-order.md) for the current dependency-ordered execution plan and codebase assessment. This document remains the full requirements reference.

October 16 interface amendment: the supplied **Unified Television Interface Design Specification** guides the default experience: source-focused Home, poster-focused Library and Home/Library/TV Guide/Music/Games destinations. These replace the older default tab/shelf placement examples below. The user's clarification makes every such choice configurable, including tabs, hierarchy, arbitrary colors, component presentation and personality. [interface-design.md](interface-design.md) records the customization matrix, default design and implementation sequence; it takes precedence over older default-layout examples. Kodi retains backend ownership.

October 17 Studio amendment: the supplied **Isolde Studio** document (Version 5, three parts, 41 sections) extends the interface commitment into a full Home Screen Studio and universal grid builder: a configurable two-dimensional grid with span and collision rules on every screen, Simple/Studio/Advanced editing modes, five workspaces over one layout model, three-layer typed components, a collection-query binding layer, a versioned action/variable model, templates/symbols/tokens, layered GIF-capable backgrounds, motion categories and a canonical versioned JSON layout schema with migration rules. It supersedes home-screen-only assumptions while preserving the contracts above: safe defaults, no required customization, no startup rewrite of existing layouts, Kodi media ownership and the Generator Test. [studio-builder.md](studio-builder.md) records the consolidated requirements, corrected module mapping (the plan's cited `profile-layout`/`layout-repository` files do not exist; the real modules are `src/home/HomeLayout`, `ComponentSchema`, `HomePages`, `HomeSkins`, `PageData` and `ActionDispatcher`), the proposed M0–M8 build modules and the acceptance highlights. Items there are requirements, not implemented capability. The milestone remains **0.0.1.2**; the plan's proposed 0.0.1.2/0.0.1.3 Studio version split is not adopted here and remains an [implementation-order.md](implementation-order.md) decision.

Requirements incorporated October 15, 2026 from the user-supplied **Isolde Master Plan Managed Capture Updated** (19 sections). The original attachment remains the source design reference. This roadmap supplements, not replaces, development-plan.md and its verified implementation history. The milestone remains **0.0.1.2**, despite the attachment's older 0.0.1.1 heading. Items below are requirements, not claims of implemented capability.

## Product contracts

- Everything is customizable; nothing requires customization to work. The default dashboard is a preset, not a hardcoded arrangement.
- Generator Test: cold boot, local media, saved artwork, local search, installed subtitle packages, input control, layouts and recovery must work without internet. Online services degrade independently. No promise of offline external authentication, subscription streaming or DRM games.
- Kodi owns media, watched state and resume state. Isolde owns presentation and navigation. Changing frontend must preserve the active user's Kodi skin, settings, sources, add-ons and library. Do not edit Kodi databases or skin settings to simulate integration.
- Existing organized media stays where the user puts it. Scanning does not move, rename or clean it. Existing folders and subfolders are supported without importing scattered files into a new arrangement.
- Required local backends are managed by Isolde: Kodi for media, GStreamer for capture. They must fail independently of the shell. Installing a backend does not imply hardware readiness.

## Requirements by source section

### 1–3 Default home, reusable widgets and navigation

Default: editable top bar (identity optional per earlier user direction), search, settings and clock; Home/Live TV/Movies/Shows/My Library tabs; eight configurable HDMI inputs; apps (YouTube, Spotify, Steam, Kodi, All Apps); compact guide; Continue Watching, Recently Added, Movies, Shows and custom shelves. Eight upstream switch inputs must not be confused with eight capture devices, and unavailable hardware must not be represented as working.

Reusable widget families: launcher, media shelf, guide, hero and information. Each declares its source, layout, sorting, limits, visibility and focus behavior. Pages/tabs/rows/panels/docks/tiles are layout data. Tabs support rename, reorder, hide, delete and submenus. Poster grids, landscape cards, compact lists and heroes share adapters. Preserve focus and scroll across playback and guide overlays; retain Settings and Home access.

### 4–5 Collections and library setup

Collections are reusable definitions independent of widgets: manual (titles, Kodi tags, playlists, nodes), metadata rules, and suggestions. One collection can power multiple shelves and tabs. User curation and explicit tags take precedence over metadata categories; uncertain classification stays in general browsing. Enrichment is optional and later.

Independent exclusions from general Movies and Shows affect browsing only. They do not delete files, change Kodi records, or implicitly affect Search, Continue Watching or Live TV. Items may belong to multiple curated collections.

Automatic Library is the default onboarding template; Curated Library discovers/imports existing distinctions such as Anime, Cartoons, Kids and Musicals. Neither is a separate feature tier. Test both meticulous and poorly organized libraries.

### 6 Guide

Compact three-to-five-channel widget, expanded overlay preserving background navigation, and fullscreen EPG with groups, schedule, details and tune actions. Validate the user's actual PseudoTV setup. Prefer supported PVR interfaces where applicable; generated EPG/playlist output requires a separately tested adapter. Never present illustrative programming as live data.

### 7 Visual editing

Contextual Edit Home Screen mode with remote movement, duplicate, hide, resize, source and appearance selection; mouse optional. Source choices include compatible Kodi playlists/tags/nodes/collections. Configure sort, item count, artwork, labels, filters and actions. Live preview, named presets and restore defaults. Preserve existing tile transparency, arbitrary PNGs, labels, roundness and category requirements.

### 8 Dual frontends

Isolde frontend presents dashboard/library and uses Kodi playback. Kodi frontend presents the user's existing skin and native dialogs. Keep Open Kodi for administration and unsupported features. Same active-user Kodi home, no competing instances or database copying on a mode switch. Stop playback/scanning before a clean handoff; do not force-kill merely to apply a mode preference. Separately importing an existing desktop Kodi installation requires an explicit backed-up migration flow, not an implicit fresh profile or overwrite.

### 9–12 Architecture, presets and acceptance

Separate source adapters → collection membership/filtering → presentation → interaction/action dispatch. Missing sources remain editable mappings, not fatal layouts. Everyday, Cinema and Family are layout/filter presets, distinct from Isolde's existing isolated user accounts; Family layout is not parental-control security.

Build component configuration/focus infrastructure, default preset, Kodi media adapter, collections, guide adapters and editors. Dual frontend preservation is brought forward because the user explicitly requested it. Every feature must support remote navigation, recovery without a functioning home button, Kodi failure isolation and transferable source mappings. Support both a pinned inputs row and a persistent source dock; final default placement remains open.

### 13 Universal search and availability

Search opens over any view and preserves focus. Remember a visible Web ON/OFF toggle. OFF is enforced at request dispatch: Kodi-only results, zero third-party search/metadata/provider calls, and no cached online results. It does not disable unrelated networking. Empty state: “Not found in your Kodi library.”

ON adds external discovery and regional offers. Match external IDs first; cautious title/year/type fallback must not merge remakes. Kodi determines local ownership/playability/progress. Separate Watch Locally, supported Watch on connected service, and informational Available on service. Installed app is not a subscription or entitlement. Require supported launch routes, freshness/region/attribution/terms review and honest unavailable/stale states. Evaluate TMDB and licensed availability sources; no provider chosen or entitlement assumed yet.

Modules: controller, Kodi adapter, online discovery, identity matcher, availability, launcher, optional watchlist/cache. Start with local titles/shows/playlists/tags, direct playback and zero-outbound tests. Then online disambiguation and provider actions. Switching OFF cancels external work and suppresses late/cached online results.

### 14 Isolde Linguist

Optional offline subtitle workflow: existing human English track first; translate existing original-language subtitles second; explicitly enabled audio extraction/transcription/translation third. OCR is later research. Labels: Find Subtitles, Translate Subtitles, Generate Subtitles, Subtitle Queue, Edit Subtitles. Identify provenance, language and machine-generated quality limitations.

No automatic full-library jobs. Generation OFF by default; online subtitle providers separately opted in and OFF by default. Models/packages download only during explicit setup. Never transmit media/audio/transcripts/subtitles during offline processing. Preserve original files; use non-overwriting Unicode SRT outputs beside writable media or managed cache for read-only media. Supported Kodi subtitle loading is required.

Evaluate FFmpeg, faster-whisper and offline translation packs against actual Japanese/Korean/Mandarin/Cantonese samples; language coverage, timing, quality and CPU cost are not assumed. Roadmap: existing-SRT translation; missing-subtitle generation; season queues with pause/cancel/retry and review, gaming-aware scheduling/glossaries; optional OCR. Offline acceptance includes cache playback, original-file preservation and completed-output retention after cancellation.

### 15–17 Offline operation, direct playback and component model

Local movie/episode selections play or resume immediately, without provider choosers. Series selections open episodes unless explicitly Play Next/Continue Watching. Context menu retains Details/Other Ways to Watch. “Always Play Locally When Available” defaults ON for web discovery; turning it OFF must not change ordinary library direct playback.

Universal actions: Play, Resume, Details, Open Collection, Change Tab, Launch App, Switch HDMI, Expand Guide, Settings, Automation and Back, with select/long-press/context bindings. Validate schemas and actions, isolate widget failures, version local configuration backups, undo and reset. No arbitrary shell execution via imported layout actions.

Simple remote editor plus Advanced composition of nested layouts/docks/navigation/templates/visibility/source mappings. Import/export layouts, themes and presets separately from media and account data. Missing bindings stay repairable. A layout with every launcher deleted must still have a recovery route. Offline recovery uses prepared installation media and locally stored settings/models/metadata.

### 18 Seamless source sessions

Isolde-owned video surface; no external capture/player UI or desktop flash. Default Expand from originating tile, configurable approximately 200–350 ms; also Fade, Slide, Instant/reduced motion. Start switching and capture concurrently, not after animation. Show neutral/device artwork until a new valid signal is ready; never mislabel an old frame as the newly selected source.

Source Session Controller: Idle → Switching → Awaiting Signal → Playing → Returning, plus timeout/error/recovery. Capture, rendering, audio, RS-232 and remote routing are separate adapters. Configurable labels, fallback art and return behavior. Home restores originating focus/scroll, or a stable fallback if removed. Errors provide Retry/Configure/Back within Isolde.

### 19 Managed capture and installation

Provision Kodi and GStreamer runtime/plugins on supported Ubuntu; capture is installed even without attached hardware. Status distinguishes missing backend, incomplete runtime, not configured hardware, signal loss and active session. Settings → System → Components shows version/health and eventually guided repair/reconfigure. Settings → Inputs → Capture provides V4L2/audio discovery, auto/manual selection, formats/FPS/audio probe and eight-input RS-232 mapping to the expected capture device.

Offline image must contain tested compatible dependencies, integrity manifests and user-controlled updates; an online apt bootstrap is not an offline installer. GStreamer is default, OBS diagnostic only. Keep replaceable backend API. Actual latency, resolution changes, frame pacing, audio sync, hotplug and Home return require real hardware qualification. HDCP limitations are not bypassed or advertised away.

## Implementation slices and release gates

October 17 capture software foundation: a device-scoped Qt Multimedia controller now discovers UVC video/audio devices and serial ports, provides an Isolde-owned preview, stores eight input labels, validates bounded RS-232 command templates, and separates frame arrival from user-confirmed source selection. Settings → System → HDMI Inputs provides remote-operated setup; Home defaults expose eight unavailable-until-configured input actions; capture-mode apps open the same unassigned input picker. Components links directly to setup. Qt Multimedia and SerialPort dependencies are included in the installer. No physical capture card or switch is available here: serial protocol compatibility, real signal correctness, latency, audio sync, hotplug and long-duration stability remain release gates. See [capture setup](capture.md). Do not claim hardware support from package presence or frame arrival alone.

October 17 universal local-search foundation: the global Command Center now queries Kodi title filters for Movies, Shows, and Episodes, merges saved rule names, pages each result type independently, and routes local results through Kodi playback/show navigation. Queries are debounced and cancel on close or replacement; there is no outbound search/metadata request path. C++ regressions cover the Kodi query mapping and navigation contract, but require execution in the Linux Qt build environment. Tag and playlist result adapters, manual memberships, and optional external discovery remain incomplete.

October 17 curation progress: Menu on movie/show details now creates a profile-local same-type manual collection or adds/removes that item. Collections hydrate bounded ID pages through strict read-only Kodi movie/show detail methods and can produce Home shelves with validated playback/show navigation. No Kodi records or media files are modified. Tests cover the bridge boundaries and fake-Kodi membership/shelf flows; CTest and real-Kodi acceptance remain outstanding. Episode manual memberships, playlists/nodes, exclusions, and curated setup templates are not complete.

October 16 discovery progress: local movie-title search, Continue Watching, and profile-specific named title/tag/genre rule collections are implemented and tested against real Kodi. Remote creation/rename/removal and bounded full-library queries are in place. October 17 working-tree changes add saved title/tag/genre rules for TV shows and Home show shelves with validated membership-based navigation; legacy definitions remain Movies. Focused regression tests are added but await execution and real-Kodi acceptance. This is still the first collection adapter, not completion of sections 4–5/13: manual memberships, playlists/nodes, exclusions, episode/universal search and optional web discovery remain open. Next substantial slice: manual collection membership and browse-only exclusions across Kodi Movies/Shows, followed by Home-shelf acceptance against a real profile.

October 16 progress: remembered read-only media access, real nested-file discovery, paged movie browsing and direct managed local playback are implemented. Live VM testing proves synthetic fullscreen playback, remote pause and Home stop/return on the XWayland path; automated coverage includes ownership, missing media, startup cancellation, paging, profile invalidation and notification delivery. This does not close all of slice 2: existing-desktop migration, broad skin/add-on/watch-resume qualification and real audio/codec hardware remain open. Next feature-sized work should prioritize local search/collections and continue-watching navigation, followed by the component/action schema and capture controller; preserve the release gates below.

1. Current: frontend-mode persistence and clean-exit switching, same-profile ownership guards; required backend dependency list and offline health inspection. Tests must distinguish infrastructure from validated end-user playback/capture.
2. Preserve customized Kodi round trip with a real non-default skin, sources, add-on settings, collections and watch/resume state. Explicit backup/import for existing desktop Kodi. Finish remembered folder grants, nested-folder discovery and direct playback.
3. Versioned component/action/source schemas and renderer, focus restoration, default dashboard and recovery; compatible migration of current layouts.
4. Collection and local-search adapters with offline request-boundary tests; curated/unorganized fixture libraries; direct playback preferences.
5. Capture controller and integrated surface with fake-backend state-machine tests, local device probes and guided mappings; then real RS-232/capture hardware acceptance. No live-capture claims from VM-only tests.
6. Guide/PseudoTV qualification; editors, import/export and presets; online discovery as an opt-in enhancement.
7. Linguist staged evaluation and implementation; offline dependency image and disaster-recovery qualification before release.

Across all slices: Generator Test, remote-only interaction, profile separation, no media reorganization, honest errors, independent backend recovery, and preservation of the user's Kodi setup.
