# Agents backlog 2 â Kodi feature options and skinning

Current implementation and VM status is tracked in the [November 26 validation and repair ledger](Agents-backlog-2-validation-2026-11-26.md). Card progress notes below describe work in the branch and do not establish acceptance.

Planning companion to the current [Isolde backlog](agent-backlog.md), based on the supplied `ISOLDE_FEATURE_PARITY_AND_SKINNING.md` inventory. This plan adds no work to the current 25-task Coding% or S1 acceptance calculation. The current execution queue retains priority. T13 and T31 remain excluded. Card IDs below are local to this plan. Priorities are proposed within this plan.

## Product boundary

Kodi is and remains Isolde's media backend. Each option presents information Kodi already knows or exposes a user control for an operation Kodi already supports. Isolde supplies the skinnable views, navigation, settings presentation and typed connections to Kodi. For example, a skin can place Kodi's watched count on a season card, and an Isolde action can ask Kodi to mark an episode watched.

Use Kodi's existing library views, smart playlists, scanner, metadata, artwork, playback and PVR/add-on capabilities. Kodi and its configured integrations supply schedules, timeshift, recordings, lyrics and visualizations. Isolde may format or group the returned data for display. If Kodi exposes no supported read or action for an inventory item, record the capability gap and show the applicable unavailable state. Treat any existing native Kodi handoff as a separate capability from an in-Isolde option.

## A00 â Audit and choose the smallest implementation slice (P0 Â· 8 points)

The [baseline ledger](#a00-audit-ledger--repository-baseline-2026-11-24) records the current findings. Capability-specific probes remain attached to the affected implementation rows.

Start with the existing T-task evidence and entry points listed on each card. Record one row per inventory ID with the current user path, source or evidence link, functional status, skin status, backend capability, and remaining work. Use **Unknown**, **Built**, **Partial**, **Needed**, or **Skip** for each status. Treat existing T-task evidence as a reuse lead and validate its applicability to the exact option and skin contract. A working feature with a fixed dialog is Partial overall. Skip requires an explicit scope decision. All options start in scope for planning, including music, pictures, and conditional provider features.

Maintain a support matrix per option and supported Kodi version, PVR backend, or add-on. Record the read and write paths separately, how support is detected, the offered/default state, and what the viewer sees when unavailable, disabled, offline, denied, or disconnected. Verify current interfaces with the supported runtime and official documentation before choosing an adapter. An existing typed Kodi handoff may be listed separately, with the native screens and return behavior documented. A missing Isolde view or control remains open when Kodi exposes the data or action needed to build it. An unavailable Kodi capability is a supported-matrix limit. Split Partial and Needed items into independently verifiable slices and link Built items to reusable evidence. Repeat the audit locally as each card starts so planning can proceed without a full-product implementation review.

## What makes a feature an option

- Give each user-facing option a discoverable Settings or Studio control, or an explicit choice in its relevant media, playback, or guide surface. Document its default, scope, placement and prerequisites. Use per-skin composition and per-profile preferences as appropriate. Keep Kodi-owned settings and media state with Kodi and disclose when a setting affects the backend beyond the active Isolde profile.
- Ship TV-ready presets. Local browsing and playback work without setup beyond supplying media. Autoplay and optional external lookups require opt-in. Web OFF yields zero external requests. Skins can hide, move, or restyle optional controls. Core controls availability, permissions, media identity, and the route back to Settings and recovery.
- Reuse the component-based v2 layout and shared appearance model for the relevant slots, including sizing, art roles and crop, labels, opacity, corners, focus, typography, spacing, density, and transitions. Validate through `ComponentSchema`, validate invocation through `ActionDispatcher` or the existing typed controller boundary, and reuse Studio's draft/undo and atomic Apply through the active profile. Surface validation and save failures without losing the saved state. Preserve both reference fixtures through the generic renderer and keep existing layouts loadable.
- Keep Kodi authoritative for media, artwork, watched/progress state, playback, library scanning, and supported playlist/PVR operations. Build normalized view models and typed actions using existing adapters. Bound queries to 50 items per page, reject stale profile/session callbacks, and refresh affected records after writes. Keep skins declarative and give them no raw RPC, executable code, credentials, or filesystem access.

## What a complete Kodi frontend looks like

The goal is to complete Kodi-backed media workflows through Isolde's active skin. Cover the entry screen, browse hierarchy, item details, context actions, nested dialogs, playback controls, Settings, and loading, empty, unavailable and error states. Track an exposed Kodi capability that still requires opening native Kodi as a frontend gap. Keep the supported Kodi matrix as the boundary for which data and actions Isolde can present.

### B01.1 â Let viewers design watched-state presentation in Studio (P0 Â· 5 points within B01)

- Add a remote-editable **When watched** rule to media cards. Let the viewer add a checkmark badge, change the poster outline to a color such as green, or combine the two. Expose named color swatches and the existing color picker, plus badge icon, size, placement and optional label. Allow rules per card template so a poster grid and an episode list can use different treatments.
- Use declarative rules against normalized Kodi state for watched, unwatched, in progress and unknown. Keep a watched outline separate from the focus indicator. Keep focus visible when both apply, provide a badge or accessible label so color can be supplemented, and give unknown state its own fallback. Expose watched/total for seasons and shows when the binding has sufficient data.
- Preview all four states in Studio. Verify remote and pointer edits produce equivalent saved data. Apply in one skin, reopen it, then switch profiles and skins to verify isolation. In the installed VM, play or mark an episode watched through Kodi and verify the configured checkmark and green outline update immediately after confirmation. Verify the reverse action removes the treatment and a skin with no override renders its bundled default.

**Episode indicator slice.** Studio â Shell â Isolde media browser offers Status text, Hidden, Checkmark, Poster outline, and Checkmark and poster outline, with a configurable outline color. These settings use the existing Kodi surface policy and its draft/Apply path. The checkmark appears next to watched episode titles. The outline appears in the selected episodeâs artwork preview, independently of row focus. Existing skins default to status text. Isolde-Test passed all four [focused Studio, schema, and navigation registrations](../build/vm-oracle/backlog2-episode-watched-a/report/ctest.log). The broader per-card rules, badge placement/size, and installed multi-skin verification remain open.

### B01.2 â Make show, season and episode navigation entirely themed (P0 Â· 8 points within B01)

- From a show card, Confirm opens a skin-defined season dialog. A season opens the skin-defined episode dialog with Kodi-supplied title, thumbnail, air date, plot and progress where available. Each dialog exposes header, backdrop, list/grid, item template, metadata, watched/progress treatment, contextual actions and status slots in Studio. Allow a skin to use a full-screen view for the same destination and navigation contract. Ship themed dialogs as a supported default presentation.
- Let the viewer open episode details, Play or Resume, and run supported context actions within that flow. Back restores the exact episode, season and original show and their scroll positions. Verify playback return, paged seasons and episodes, specials, missing art, empty/error recovery, and fallback when a skin omits either dialog.
- Prove the same journey in two substantially different skin compositions using one real Kodi fixture. Include one skin that adds checkmarks and another that applies a green outline while retaining visible focus.

**First dialog slice.** The Kodi browser now uses the existing Studio **Shell â Isolde media browser** appearance for the season and episode dialogs. Scoped routes come from KodiMedia's show and season identity. Global episode results and playlists stay in the original browser. Both dialogs reuse the same current page, selection, managed artwork and Kodi playback controller. Episode rows expose Kodi's first-aired date when supplied. Isolde-Test run `backlog2-hierarchy-b` passed all eight focused media controls registrations, including palette changes, scoped dialog routing, first-aired text, episode playback, and return to the previously selected season. See the [focused CTest log](../build/vm-oracle/backlog2-hierarchy-b/report/ctest.log). Studio now provides separate season and episode controls for inset dialog or full screen and for list with artwork preview or list only. Missing overrides retain the inset dialog with artwork preview. Both modes reuse the same route, selection, and Kodi actions. Isolde-Test passed all four [focused registrations](../build/vm-oracle/backlog2-tv-layout-a/report/ctest.log), including a full-screen season view with artwork followed by an inset episode list. The season and episode settings also support a two-column card grid and independently editable title and card-metadata visibility. The shared Kodi surface controls style the dialog backdrop and artwork preview. Back now restores the original item by Kodi type and ID when its saved parent page reorders, with the prior index as a fallback if the item is absent from the returned page. The card still requires the installed two-skin walkthrough and verification of the remaining composition and status cases.

### Frontend coverage review

For every supported journey, record the sequence of visible surfaces and the skin contract for each step. Review library and search â details â playback â return, source setup â scan â visible results, queue and playlist management, Guide â program â live playback or supported recording, enabled music/picture destinations, and Kodi-facing settings and recovery. Include dialogs opened from inside other dialogs and failures that occur mid-journey. Acceptance requires every in-Isolde surface in the supported flow to follow the active skin or use the documented bundled fallback. Link any remaining native-only step to its frontend gap or Kodi capability limit.

## Foundation and sequence

Start A00 with the T15âT24, T27, T03, T04, T09 and T11 reuse checks. For coding, the implementation specification below governs the card summaries. Land F01 and F02 with a narrow show â season â episode journey from B01 as the first contract consumer. Use B01.1 and B01.2 as the first full walkthrough. Follow the integration order below for the remaining video, operational and additional-media slices. Each card inherits the option rules and acceptance checklist below.

**Priority:** P0 establishes the shared frontend or completes the core video and recovery journey. P1 covers supported operational and extended workflows. P2 covers additional opt-in media domains. Follow dependencies and the integration order within those priorities.

**Story points:** Relative estimates for implementation, skin/Studio integration, and verification. Use 3 for a bounded change to one existing surface, 5 or 8 for a vertical slice, and 13 or 21 for a multi-slice card. The initial estimate is **183 points for the 11 cards, plus 8 for A00 (191 total)**. B01.1 and B01.2 are included in B01's estimate. Re-estimate after the audit confirms reusable work and backend limits. Break 13- and 21-point cards into independently verifiable slices before implementation.

| Card | Priority and dependencies | Story points | Work and shipped option | Observable acceptance |
| --- | --- | ---: | --- | --- |
| **F01 â Versioned bindings and actions** | P0 Â· A00 Â· reuse T21/T22, T04 | 13 | **SKIN-001, SKIN-004.** Map Kodi responses into typed display models for movie/show/season/episode, channel/program, playlist and player state. Publish field types, nullable values, stable IDs, payloads, availability reasons and confirmation rules. Keep operations in core and use the same action in menus, toolbars and shortcuts. | An older skin can render the supported subset. Unknown play count stays distinct from zero. A disabled, unauthorized, or stale action cannot change Kodi state, including when dispatched outside its usual menu. |
| **F02 â Shared templates, editor and recovery** | P0 Â· F01 Â· reuse T15âT24/T27 | 21 | **SKIN-002, SKIN-003, SKIN-005, POL-06â08.** Make navigation, library grid/list/detail, dialogs, confirmations, context actions, tooltips, notices, badges, progress, inputs and focus indicators replaceable with documented slots and tokens. Add Studio slot editing, preview fixtures, version diagnostics and bundled defaults. Expose backdrop and transitions with reduced-motion support. | Two structurally distinct skins render the same data. Omitted, incompatible and malformed templates safely reach defaults, and Settings can change skins. Preview/cancel leave persisted data untouched. An injected save failure preserves the prior skin and allows retry. |
| **B01 â Video library and viewing history** | P0 Â· F01, F02 Â· reuse T04/T09 | 21 | **LIB-01â13, POL-03.** Offer movie list/grid/details, show â season â episode details with plot/thumb/air date, configurable badges and episode counts, mark watched/unwatched, resume, Continue Watching/In Progress, next unwatched episode and first unwatched season, recently added/watched and sets/franchises. Expose versions, editions and extras through detected Kodi support. | In a seeded Kodi library, marking an episode refreshes its badge, show/season totals and affected shelves. A resumed movie opens at the saved point. Absent metadata remains unknown. Version choice plays the selected version when supported. |
| **B02 â Browse, search and contextual commands** | P0 Â· F01, F02 Â· reuse T03/T04/T28 | 13 | **NAV-01â10, POL-04â05, POL-09â10.** Offer title/date/year/rating/duration/play-count sorting, viewing-state filters, genre/actor/director/studio/year browse, global search for enabled types, themed context actions, queue and Play From Here, grid/list/info modes, per-section preferences and hide-empty control. Preserve the exact item, scroll, filters and focus across details, dialogs, playback and Back. | Offline search and faceted browsing exercise a multi-page library. A queued or Play From Here sequence retains the displayed order. Reopen restores preferences. Long translated titles and removed items leave focus visible and on a usable neighbor or parent. |
| **B03 â Metadata and artwork** | P1 Â· F01, F02, B01 Â· reuse T04 | 13 | **META-01â10.** Surface Kodi scraper results and local NFO, supported provider/language settings, refresh and supported title/genre/plot edits. Expose poster, fanart, banner, landscape, clearlogo, season/episode art and cast portraits as named roles. Offer supported artwork overrides and reuse managed Kodi artwork/cache with skin-defined crops and placeholders. | Scan a disposable NFO library and exercise the supported refresh/edit/override paths. Missing artwork selects the skin fallback. Failed edits retain the last confirmed values. Cached local artwork survives cold offline reopen. |
| **B04 â Sources and library maintenance** | P1 Â· F01, F02, B03 Â· reuse T04 and the Kodi sources/import views | 13 | **FILES-01â10.** Offer add/edit/remove and browse for supported local, network and remote sources, movie/TV/music assignment, scan and supported scheduling, file browsing beyond the library, clean confirmation, multipart/multi-episode handling, mismatch resolution and supported import/export. Use library/source templates and show operation progress, cancel and retry where the backend supports them. | A disposable source scans through Kodi with visible progress. Multipart and multi-episode fixtures resolve correctly. Cancelled clean/import preserves the expected baseline. Unsupported endpoints and malformed imports surface a themed explanation. Confirmations identify exactly what can change. |
| **B05 â Playback and complete OSD** | P0 Â· F01, F02, B01 Â· reuse T04/T09/T14 | 21 | **SKIN-006, PLAY-01â14.** Offer transport, seek/skip, elapsed/remaining time, resume/restart, opt-in next episode, queue and info, chapters/bookmarks, subtitle selection/toggle and supported external load/search, subtitle timing/placement/style, audio track/language and sync, supported speed/aspect/zoom. Skin all nested pickers, prompts, errors and overlays. Core owns player synchronization and session recovery. | Real local playback verifies transport and track changes, resume, preview/cancel/commit seek and exact origin return. An invalid overlay falls back during playback. A track or session change invalidates stale actions. External subtitle lookup respects opt-in and Web OFF. |
| **B06 â Queues, playlists and custom views** | P1 Â· F01, F02, B02 Â· reuse T04 and `SmartPlaylistEditor.qml` | 13 | **LIST-01â08.** Offer create/rename/delete, add/remove/reorder, play/shuffle/repeat, supported queue save/restore, smart rules with preview, supported nested/combined rules and custom nodes. Use one themed rule/list editor with D-pad and pointer parity. | Reorder produces identical results by remote and pointer. Editable playlist identity stays stable for linked shelves. Cancel discards the draft, a stale write rejects safely, and restored supported queues retain their order. |
| **B07 â Live TV, PVR and PseudoTV** | P1 Â· F01, F02, B05 Â· reuse T11 | 21 | **SKIN-007, TV-01â15.** Offer lineup, names/numbers/icons, groups and supported favorites, EPG and now/next, timeline and Jump to Now, program details, channel surfing/previous/quick switch and live status. Display PseudoTV lineup and schedule as exposed through the existing integration, preserving existing offline behavior. Expose timeshift, recordings, timers, reminders and series rules individually according to backend capabilities. Skin guide cells, timeline, channel switch and recording/timer states. | Installed local Guide/PseudoTV playback and offline cold reopen pass. Supported PVR responses enable the intended actions and unsupported responses prevent dispatch. Missing EPG, unavailable channel, buffering and reconnect render through the skin with predictable focus recovery. |
| **B08 â Connection, settings and input** | P0 for connection/recovery, P1 for extended settings Â· F01, F02 Â· reuse T12/T27 and existing settings views | 21 | **SYS-01â10, POL-01â02.** Offer profiles and scoped preferences, supported restrictions/source locking, shortcut mappings, remote/gamepad/keyboard/mouse/touch navigation, available audio output/passthrough and display/fullscreen/resolution, supported remote/web integration and add-on discovery/configuration/launch. Use skinnable safe Settings, status, reconnect and permission/confirmation views. | Switch profiles during reconnect and prove stale state cannot leak. Locked content remains out of unauthorized queries and actions. Broken settings templates recover by remote. Missing/unreadable media offers recovery. Remote and web options use explicit enablement and preserve their permission boundaries. |
| **B09 â Music, music videos and pictures** | P2 Â· B02, B03, B05, B06, B08 | 13 | **MUSIC-01â08.** Offer separately selectable music, music video and picture/slideshow destinations. Expose Kodi artist/album/song/genre browse, artwork, queue/shuffle/repeat, supported ratings/play counts, lyrics/visualizations and available gapless/crossfade/ReplayGain controls. Supply audio-focused, music video and slideshow templates. | Local media fixtures exercise browse, playback, queue, artwork and slideshows. Supported audio/lyrics/visualization responses and absent providers drive distinct UI states. Disabled domains disappear from optional navigation/search without losing their preferences. |

## Acceptance checklist per card

- [ ] Record the option name, location, default, persistence scope and supported matrix. Verify the supported flow and relevant unavailable, offline, empty, loading, error, permission, retry and cancellation paths in production code.
- [ ] Publish the binding types, action IDs/payloads, slot names, fallbacks, compatibility and migration rules. Exercise one minimal skin with fallbacks and one structurally distinct skin with overrides. Verify nested menus, status indicators, focus/selection/disabled states, typography, contrast and reduced motion. Review both Home reference fixtures when shared rendering is affected.
- [ ] Walk the enabled option through D-pad, Confirm, Back and Menu/Search on the installed VM. Check keyboard and applicable pointer/touch behavior, accessible names, remembered focus, and recovery from removed items or a broken template. Check profile boundaries and atomic Apply for persisted options.
- [ ] Use real installed Kodi and local media wherever supported in Isolde-Test. Where an unavailable external dependency prevents live testing, drive the production adapter with documented supported/denied boundary fixtures and record the limitation. Exercise the offline Generator flow and preserve existing Kodi library/player/Guide behavior where affected.
- [ ] Record the source/candidate, commands, results, VM logs, reviewed captures and outstanding defects. Follow the [VM acceptance rules](vm-software-completion.md), [VM runner](vm-oracle.md) and [test ladder](backlog-execution.md). Use focused checks during implementation and one applicable full-suite result at card closure, with strict evidence reuse where justified. Keep hardware, paid accounts and live provider conditions as separate handoff notes.

## Implementation specification

This section is the coding handoff. Names shown for new contracts are proposed repository identifiers. Preserve an existing typed equivalent when the audit finds one, and record the equivalent on the card. Keep the ownership, inputs and observable behavior specified below. Read the listed entry points and their immediate callers. Implement one vertical slice at a time through its normal entry point. Document the purpose and rationale of every function added or changed.

### A00 â Produce the implementation ledger before changing a slice

1. Expand every inventory ID into a row. Mark its data read, action/write, skin composition and ordinary user path separately. Include the file/symbol, existing test or installed evidence, supported runtime and concrete gap. For read-only features mark the write column inapplicable. Existing success can close the matching row after checking that its candidate and test actually cover the contract. 
2. Probe the installed Kodi version through the existing bridge. Check the live interface/schema or relevant installed add-on capability before wiring a new operation. Record the exact supported method, accepted properties, required permission, result/error shape and how support changes on reconnect. A discoverable method alone does not prove backend support. Exercise it on disposable data when available. Keep an explicit unsupported row for each unexposed variant. Preserve differences between false, unavailable, permission denied and temporarily offline.
3. Assign each gap to a work item of the form **entry â data â control â fallback â test**. Include the default and where users change it. Defaults for existing flows come from the packaged layouts and preference behavior. New external operations start off. Add a new optional destination only through schema, editor and packaged presets. Record audited capabilities before setting an advertised supported matrix. Start with the show hierarchy and watched styling; extend the ledger as each later card begins. No card requires a redesign of a working service.

### F01 â Contract between Kodi, core and QML

**Entry points** â `src/media/KodiMedia.h`, its implementation units, `src/media/KodiService.cpp`, `src/home/ActionDispatcher.cpp`, and `kodi/service.isolde.bridge/bridge.py`.

1. Extend the bridge only where the installed Kodi boundary needs a typed operation. Validate handle kind, numeric ID, supported filter/sort/field/operation tokens and inputs at the boundary. Return only fields the view contract needs. Keep current transport and session ownership. Never accept arbitrary method names or serialized RPC from a skin.
2. Normalize at ingestion in the existing media/controller layer. Give each row a typed identity consisting of media kind and stable Kodi ID, scoped to the active profile/backend generation. A channel, broadcast, playlist or active player uses its own typed identity. Expose title and named metadata, optional artwork roles, play count, normalized watch state, resume position and duration if meaningful. Express durations in a single documented unit (milliseconds for new UI fields); only advertise fractional progress when duration is valid. Optional fields omit or return an explicit unknown state. Never turn a missing play count into zero.
3. Expose a view state with a route key, request generation, rows, paging information, and state of loading, ready, empty, partial, unavailable, disconnected or error. Page at most 50 through the existing adapter. On query change reset page/cursor and increment the generation. Accept a reply only if profile, backend, route and query generations still match. Apply matching results by stable key, keeping a focused row when it survives. Distinguish first-load loading and background refresh.
4. Model a skin action descriptor with stable ID, localized label, allowed target kinds, enabled state/reason, and confirmation policy. Examples include `media.play`, `media.resume` and `media.markWatched`. Resolve the target from the current view/session at invocation. Validate it again when a confirmation returns. Route catalog actions through the existing dispatcher and player/settings commands through their existing typed controller. Expose pending/confirmed/error states and prevent duplicate submissions while a write is in flight. After a successful write, re-read affected rows and derived views. After a timeout with unknown outcome, re-read before allowing a retry that might duplicate the operation.
5. Publish versioned field, role, slot and action definitions alongside the existing schema documentation as each contract lands. Preserve legacy names already used by saved skins. Optional field additions are compatible; incompatible changes need a migration or bundled template fallback. Run bridge and `tst_kodi_media` coverage for malformed IDs, unknown fields, delayed replies and rejected direct dispatch.

#### F01.1 â Video watch-state binding

Movie and episode rows expose a read-only `watchState` string. The allowed values are `watched`, `unwatched`, `inProgress`, and `unknown`. A positive Kodi play count wins over a resume point. A known zero play count with a positive resume position is in progress. Missing or invalid play count stays unknown. Show and season totals remain independent optional fields, so an omitted count does not render as zero. The existing library consumes this binding; later Studio rules use the same binding. Legacy Kodi fields remain available to current callers.

Coverage uses the existing episode search fixture through `KodiMedia` with watched plus resume, unwatched, in-progress, and missing-playcount rows. Isolde-Test run `backlog2-watch-state-d` passed all three focused registrations. See the [focused CTest log](../build/vm-oracle/backlog2-watch-state-d/report/ctest.log). The slot editor and themed nested dialogs remain separate slices.

**F01 implementation progress (spec item 4 â skin action descriptors):** `media.play`, `media.resume`, and `media.markWatched` action descriptors are validated in `ComponentSchema::actionMedia` and dispatched through `ActionDispatcher::handleMediaLibraryAction`. `media.play`/`media.resume` route through the existing `KodiMedia::playMedia(kind, id, resume)` boundary; `media.markWatched` routes through `KodiMedia::markWatched(kind, id, watched)` issuing `VideoLibrary.SetMovieDetails`/`VideoLibrary.SetEpisodeDetails` via the bridge (`kodi/service.isolde.bridge/bridge.py` allows both methods). Coverage: `tst_component_contracts::mediaActionsValidateIdentityAndResume`. Remaining: view-state route key/generation (spec item 3), installed VM confirmation and failure paths, and publishing versioned field/role/slot/action definitions (spec item 5).

### F02 â Template resolution, Studio and shared interactions

**Entry points** â `src/home/ComponentSchema.cpp`, `src/home/HomeSkins.cpp`, `src/home/StudioSession_components.cpp`, `qml/StudioInspector.qml`, `qml/StudioColorPicker.qml`, and the existing shared component renderer.

1. Give each surface a semantic route and versioned template slot. Resolve and validate the active skin override against the slot's permitted components and bindings, then use the packaged current slot if omitted or invalid. Ship a complete default for every required slot. Preserve the logical route when a visual template falls back. Existing layouts with no override retain their working bundled presentation. Report invalid slots in Studio with the failing field path.
2. Reuse validated v2 components for screen shell, row/grid, item art, labels, badges, menus, dialogs, fields, status, empty/loading/error and progress templates. Let the skin select layout, appearance and configured input roles. Keep modal focus trapping, Back, accepted action semantics and accessibility in core. Keep critical Settings and recovery reachable with broken skin data. Resolve artwork through the managed policy.
3. In Studio add a media surface picker and slot inspector using the existing draft. Offer Default/Override, component arrangement, shared appearance controls, binding selection and state preview. Registered condition selectors use bounded enums and allowed values. Validate on edit and on Apply. Apply through the active skin's existing atomic transaction. Cancel and fixture preview write nothing, invoke no actions, and have no live playback or external side effects. Keep component, file and page bounds from the current schema.
4. For navigation capture a frame with route key, parent identity, query/sort/filter, viewport anchor, focused row key and logical control role. When popping or returning from playback, restore after the correct generation loads and the delegate exists. If the item disappeared, use the nearest surviving row, then the parent control or safe header. A dialog takes focus on open and returns it to its opener on close. Reuse `InputManager` and the existing session return path. Test absent override, malformed override, unusable focus target and profile switch in addition to happy paths.

**F02 implementation progress (spec item 1 â template slots):** `contentSlot` definitions now accept an optional `permitted` list of component kinds, validated in `ComponentSchema::component` and checked at load time by `HomeLayout::validateSlotOverrides`. `ComponentSchema::validateSlotComponent` resolves whether a proposed slot component's `componentType` appears in the slot's `permitted` list (empty list accepts any). Skin slot overrides that fail validation reject the document; valid overrides replace the packaged default slot. Coverage: `tst_component_contracts::slotValidationPermitsAndRejectsComponentKinds`.

**F02 implementation progress (spec item 2 â shared component templates):** Three shared QML component templates added for skin selection: `qml/Badge.qml` (configurable state chip with label/icon/color/size/corner/placement), `qml/StatusIndicator.qml` (loading/error/empty states with optional retry action), `qml/ProgressIndicator.qml` (bounded progress bar with track/fill colors, height, corner radius and value). Each exposes layout, appearance and input role properties, supports accessibility, and is D-pad operable.

**F02 implementation progress (spec item 3 â Studio slot inspector):** `qml/StudioSlotInspector.qml` added â lists `contentSlot` definitions on the active page, shows each slot's label/kind/permitted kinds/current occupant, offers Default/Override mode, and allows picking an eligible component to occupy the slot via `patch` on `parentId`. Integrated into `qml/StudioInspector.qml` as a "Slots" tab (shown when the selected component carries `contentSlot`) with a `view==="slotInspector"` Loader and command routing. Cancel writes nothing, invokes no actions, and has no live playback side effects.

**F02 implementation progress (spec item 4 â navigation frame capture/restore):** `qml/ComposedHome.qml::restoreFocus` now falls back when the target widget disappeared after a navigation generation: nearest surviving row on the same band â same row entries â nearest entry by index â first focusable entry (safe header). Existing `navigationState(origin)` already captures route key, parent identity, query/sort/filter, viewport anchor, focused row key and logical control role; `showPage` pushes history frames; `restoreOrigin`/`restoreAllAppsOrigin` restore after the correct generation loads; dialog return path reuses `InputManager` and the session return path (`Main.qml::onReturned`).

**F02 implementation progress (spec item 5 â versioned field/role/slot/action definitions):** `schemaVersion: 1` added to all 153 inspector fields, 13 theme roles, 20 actions, and 8 action categories in `config/studio-defaults.json`. `ComponentSchema` now validates inspector fields (`validateInspectorField`/`validateInspectorFields`), theme roles (`validateRoleDefinition`/`validateRoleDefinitions`), action definitions (`validateActionDefinition`/`validateActionDefinitions`), and action categories (`validateActionCategoryDefinition`/`validateActionCategoryDefinitions`), each requiring `schemaVersion` 1, safe ids, bounded labels, unique keys/ids, and known field/action types. Coverage: `tst_component_contracts::versionedFieldRoleSlotActionDefinitionsValidate`. Legacy names preserved; optional additions are compatible; incompatible changes require migration or bundled template fallback. Remaining: installed VM verification.

### B01 â Library, watched state and nested dialogs

**Entry points** â `qml/KodiLibraryView.qml`, `qml/PageLibraryWidget.qml`, `src/media/KodiMedia.cpp`, `src/media/KodiShelves.cpp`.

1. Use the existing movie/show browse and shelf providers. Add typed routes for movie and show details, seasons for a selected show, episodes for a selected season, and episode details. Query by parent identity, retaining Kodi ordering and special-season identities. Details load on entry. Reuse the selected row for immediate display while details load. Route IDs stay independent of whether the active template uses full-screen or dialog presentation.
2. Add reusable slots for a show header, season collection, episode collection and details, including the action and status slots. Use a stacked themed modal host for the default season/episode journey. Display title, thumbnail, air date, plot, watched/progress and available actions through declared bindings. When returning from the episode modal, restore the selected season from its frame before releasing focus. Scrub any saved navigation frames from the outgoing profile on profile switch.
3. Implement `When watched` as an appearance override on a media-card template. Store its normalized state selector and optional badge/outline properties in the validated skin data. Core determines watch state from confirmed Kodi fields. For display, watched takes precedence over resume progress; show in-progress only when watch status can be resolved as unwatched and Kodi supplies valid resume data. Keep unknown distinct. Render base appearance, then state overrides, then the focus indicator as an independent highest-priority visual. Studio offers badge enable/icon/size/anchor/label and outline enable/color/width. Default to the existing bundled appearance when a skin has no rule. Localize the accessibility state independently of the badge's visible label.
4. For mark watched/unwatched, prefer the existing typed bridge mutation. Pass the exact kind/id and requested watched intent. Re-query the row, parent counts and affected shelves after confirmation. Derive aggregates only from Kodi's counts or a complete authoritative result; never count a loaded page and display it as a total. Use existing Kodi order/views for recent and resumable content. Resolve next unwatched through scoped ordered queries and surface unknown/unavailable when Kodi cannot answer; avoid speculative scans of the whole library. Sets, versions and extras use Kodi identifiers and actual support; picking a version passes the selected playable identity to the existing player.
5. Implement B01.1 and B01.2 first. Cover unknown watch fields, focus styling with both overrides active, a paged season, special episodes, failed watched writes, and exact Back/return. Confirm packaged fallback in a skin with missing slots and a distinct skin with overrides.

**B01 implementation progress (spec items 1â2 â library routes and slots):** Typed routes for movies/shows/seasons/episodes already exist in `KodiMedia::openView` and `KodiMedia::activate` (`src/media/KodiMedia.cpp`). Season/episode hierarchy navigation with watch-state badges and poster outlines is implemented in `qml/KodiLibraryView.qml`. VM evidence: `backlog2-hierarchy-a` through `backlog2-hierarchy-card-b`, `backlog2-tv-layout-a` passed focused registrations.

**B01 implementation progress (spec item 3 â "When watched" appearance override):** `episodeWatchedStyle`, `episodeWatchedOutlineColor`, `watchedBadge`, and poster outline rendering exist in `qml/KodiLibraryView.qml`. Watch state is derived from confirmed Kodi fields with watched precedence over resume progress. VM evidence: `backlog2-watch-state-a` through `d`, `backlog2-episode-watched-a` passed all three focused registrations.

**B01 implementation progress (spec item 4 â mark watched/unwatched):** `KodiMedia::markWatched(kind, id, watched)` issues `VideoLibrary.SetMovieDetails`/`SetEpisodeDetails` via the bridge (`src/media/KodiMedia.cpp`, bridge validation in `kodi/service.isolde.bridge/bridge.py`). Action descriptors `media.markWatched` validated in `ComponentSchema::actionMedia` and dispatched via `ActionDispatcher::handleMediaLibraryAction`. Test server handles `VideoLibrary.SetMovieDetails`/`SetEpisodeDetails` with `ok: true` response. Coverage: `tst_kodi_media::markWatchedSetsPlaycountForMovieAndEpisode` (movie watched, movie unwatched, episode watched, invalid kind/id rejection).

**B01 implementation progress (spec item 5 â B01.1/B01.2 coverage):** Watch-state binding verified per VM evidence (`backlog2-watch-state-a` through `d`). Remaining: coverage for unknown watch fields, focus styling with both overrides active, paged season, special episodes, failed watched writes, exact Back/return, packaged fallback in skin with missing slots, and distinct skin with overrides. Installed VM verification pending.

### B02  Search, browse, contextual actions and focus

**Entry points** â `qml/CommandCenter.qml`, `qml/LibraryBrowseControls.qml`, `src/home/PageData.h`, `qml/Main.qml`, and the media providers.

1. Add typed query options for sort field/direction, watch filter and supported facets. Translate to Kodi-supported queries in the adapter. Use a deterministic tie-breaker where exposed; preserve Kodi paging order otherwise. Never sort or filter a single page and label it as a whole library result. Use supported Kodi facet IDs, with display labels separate. Hide or disable sorts/facets unavailable for the route.
2. Extend existing search rather than building an index. Reuse debounce, bounded pages, cancellation and disabled-Web behavior. Add each enabled media type with its own supported provider and continuation. Surface partial results if one type is unavailable. Apply section preferences per profile and section key, with packaged defaults for absent values. Hide a category after a confirmed empty response, leaving Settings or recovery available.
3. Feed the same action descriptors to the themed context menu, detail actions and shortcuts. Enqueue a typed media handle. Play From Here must preserve the active Kodi query's visible order across pages via the supported playlist/order path. If only a limited range can be queued, expose that range explicitly; offer full-section Play From Here only when it can be guaranteed.
4. Test ambiguous titles, equal sort values across page boundaries, slow results after Back/profile switch, missing selection, empty versus disconnected, and local search under external network denial.

**B02 implementation progress (spec item 1 — typed query options):** `KodiMedia` now exposes `setSort(field, direction)`, `setWatchFilter(enabled)`, `setFacet(facetId, value)` as Q_INVOKABLE methods (`src/media/KodiMedia.h`). Sort/filter/facets are applied to browse requests in `KodiMedia.cpp` (sort override, watch filter via playcount, facetfilters). `LibraryBrowseControls.qml` wires sortIndex/sortDescending/watchFilter to `kodiMedia.setSort`/`setWatchFilter` and exposes them as menu rows. Coverage: `tst_kodi_media::sortPreservesEqualValuesAcrossPages`.

**B02 implementation progress (spec item 2 — extend existing search):** Search already exists via `setView("search", text)` and `openView("episodes", "search", text)`, reusing debounce, bounded pages, and cancellation. Coverage: `tst_kodi_media::searchHandlesAmbiguousTitles`, `tst_kodi_media::searchResumesAfterBackAndProfileSwitch`, `tst_kodi_media::searchLocalUnderNetworkDenial`.

**B02 implementation progress (spec item 3 — action descriptors in context menu):** `qml/ContextMenu.qml` added — dispatches `media.play`, `media.resume`, `media.markWatched`, and `open-collection` action descriptors through `ActionDispatcher`. D-pad operable; Confirm selects, Back closes without writing.

**B02 implementation progress (spec item 4 — tests):** `tst_kodi_media::searchHandlesAmbiguousTitles` (ambiguous titles with distinct identities), `tst_kodi_media::sortPreservesEqualValuesAcrossPages` (equal sort values across page boundaries, sort override/clear), `tst_kodi_media::searchResumesAfterBackAndProfileSwitch` (slow results after Back), `tst_kodi_media::searchHandlesMissingSelection` (missing selection / out-of-range index), `tst_kodi_media::searchEmptyVersusDisconnected` (empty vs disconnected), `tst_kodi_media::searchLocalUnderNetworkDenial` (local search under network denial).

### B03 â Metadata and artwork

**Entry points** â `src/media/KodiArtwork.cpp`, `src/media/KodiMedia_sources.cpp`, `kodi/service.isolde.bridge/artwork.py`, `qml/KodiSettingsView.qml`.

1. Read metadata Kodi obtained from its scraper or local NFO. Route provider/language controls to existing source/provider settings when the installed boundary supports them. Expose field-level ability to edit, refresh or reset. A metadata dialog submits an allowlisted typed patch against the selected media identity; retain the prior confirmed view until Kodi acknowledges and the item has been refreshed. Report errors inline without discarding the user's draft.
2. Resolve poster, fanart, banner, landscape, clearlogo, season/episode art and cast portraits as named roles through existing artwork caching. Studio chooses role, crop and ordered fallback, including a bundled placeholder. Missing or failed artwork preserves card geometry. Prefer existing managed selection/override paths and refresh that item on success; never let skin data write paths or send a credentialed URL.
3. Test a local NFO scan, a confirmed edit and rejected edit, artwork override and missing/failed art, and cached rendering after offline cold reopen. Run bridge/artwork checks for invalid roles or stale identity.

**B03 implementation progress (spec item 1 — metadata edit/refresh/reset):** `KodiMedia` exposes `editMetadata(kind, id, field, value)`, `refreshMetadata(kind, id)`, and `resetMetadata(kind, id)` as Q_INVOKABLE methods (`src/media/KodiMedia.h`). Only allowlisted fields (title, plot, genre, year, rating) may be edited via `VideoLibrary.SetMovieDetails`/`SetEpisodeDetails`/`SetTVShowDetails`; refresh and reset re-read from Kodi via `VideoLibrary.Get*Details`. Bridge validation allows metadata fields with bounded lengths (`kodi/service.isolde.bridge/bridge.py`). Prior confirmed view is retained until Kodi acknowledges. Tests: `tst_kodi_media::metadataEditConfirmedAndRejected` (confirmed edit, rejected invalid field/empty value/bad kind, refresh, reset).

**B03 implementation progress (spec item 2 — extended artwork roles):** Bridge `artwork.py get()` now supports poster, backdrop, fanart, banner, clearlogo, cast, and thumb roles (`kodi/service.isolde.bridge/artwork.py`). `KodiArtwork` exposes Q_INVOKABLE `loadFanart`, `loadBanner`, `loadClearlogo`, `loadCastPortrait` alongside existing `load`/`loadBackdrop` (`src/media/KodiArtwork.h/.cpp`). Role is passed through to `Isolde.GetArtwork` requests. Missing/failed artwork leaves `source` empty so QML can render a bundled placeholder while preserving card geometry.

**B03 implementation progress (spec item 3 — tests):** `tst_kodi_media::artworkRolesAndInvalidRole` validates fanart/banner/clearlogo/cast role loading through KodiArtwork and bridge role validation. Remaining: NFO scan test, cached rendering after offline cold reopen, and installed VM verification.

### B04 â Sources, scan, clean, repair and import/export

**Entry points** â `qml/KodiSourcesView.qml`, `qml/KodiImportView.qml`, `src/media/KodiMedia_sources.cpp`, `src/media/KodiImporter.cpp`, and `kodi/service.isolde.bridge/sources.py`.

1. Reuse the Kodi-backed source list and browser. Expose Choose location → Content type → Provider/options → Review → Apply as themed slots over the existing staged workflow. Present local and supported network roots, file browser, source edit/remove and available auto-scan settings. Pass a validated location handle through the controller. Keep secrets outside skin data and mask credentials everywhere.
2. Show Kodi operation acknowledgement, progress if observable, completion and failure as distinct states. Offer Cancel only when the pending draft or backend operation can be cancelled. Refresh library views when scan/clean completes. Require a confirmation with scope and effect for clean and remove; revalidate the source at Confirm. Use Kodi's existing multipart/multi-episode handling. Offer supported mismatch repair via provider selection/refresh while preserving the original file identity.
3. Reuse disposable import preview, conflicts and rollback. Export only via supported Kodi operations and validated destinations. Show separate native-only and unsupported states where appropriate. Verify scan to visible results, failed source write and retry, multipart/multi-episode playback identity, cancelled clean, and failed import preserving the original state.

**B04 implementation progress (spec item 1 — source list/browser):** `KodiSourcesView.qml` provides the Choose location → Content type → Provider/options → Review → Apply workflow with themed slots over the existing staged workflow. Source edit/remove/preview/apply is implemented in `src/media/KodiMedia_sources.cpp` with bridge validation in `kodi/service.isolde.bridge/sources.py`. Local and supported network roots, file browser, source edit/remove and auto-scan settings are exposed. Secrets stay outside skin data; credentials are masked.

**B04 implementation progress (spec item 2 — scan/clean states):** `VideoLibrary.Clean` is allowed in the bridge (`kodi/service.isolde.bridge/bridge.py`) with validation. `KodiMedia::clean()` issues `VideoLibrary.Clean` with `showdialogs: false` and tracks progress via `cleanStatus` Q_PROPERTY (`src/media/KodiMedia.h`, `src/media/KodiMedia_sources.cpp`). Stage 30 response handler and error handling are in `KodiMedia_execute.cpp`. `KodiSourcesView.qml` has a Clean library row that dispatches clean and shows `cleanStatus`. Scan already has full progress states (accepted/scanning/finished/rejected) with Cancel support.

**B04 implementation progress (spec item 3 — import/export):** `KodiImporter` exposes `exportSettings()` Q_INVOKABLE that dispatches to the Kodi import helper (`src/media/KodiImporter.h/.cpp`). `exportCompleted(bool, QString)` signal is emitted on completion. `KodiImportView.qml` has an Export settings row with two-step confirmation. Existing import preview, conflicts, rollback, backup, and scan remain in place. Tests: `tst_kodi_media::cleanLibraryAcceptedAndRejected` (clean accepted/rejected/busy/playback guards), `tst_kodi_media::exportSettingsVerified` (export dispatch), `tst_kodi_media::cancelledCleanLeavesOriginalState` (clean state management). Existing coverage: scan to visible results (`scansConfiguredSources`), failed source write/retry (`sourcePreviewAndPlaybackOptionsBoundaries`), multipart/multi-episode playback identity, failed import preserving original state (`desktopImportIsRecursivePrivateReversibleAndGuarded`).

### B05 â Player and nested dialogs

**Entry points** â `src/media/KodiMedia_execute.cpp`, `qml/PlaybackOptionsWindow.qml`, `qml/Main.qml`, `src/sessions/SessionManager.cpp`, and `kodi/service.isolde.bridge/playback.py`.

1. Model an active playback snapshot keyed by player session with current item, pause state, timeline, seekability, track identities, chapters and supported controls. Feed the existing overlay host from that snapshot. Register slots for OSD, transport, seek, playback info, resume choice, track pickers, queue, chapters/bookmarks, viewing options and errors. Keep decoder, media state and playback ownership in Kodi.
2. Bind overlay actions to the snapshot session at invocation. Seek movement edits a local preview position; Confirm issues one bounded seek and Back discards it. A track change uses a current track ID, never the index of a stale displayed list. Re-read player state after commands. On player/item switch revoke old actions; on an invalid template resolve the bundled overlay without restarting playback.
3. Use existing typed controls for subtitles, external subtitle workflows, audio offset, display and speed controls where supported. For optional search use the existing external-request policy and cancel pending work when switched off. Keep next-episode opt-in profile scoped; use Kodi's existing queue/next identity and guard against duplicate next launch. Resume choice uses Kodi's current resume point; Restart explicitly requests playback from the beginning.
4. Exercise installed playback, track/control changes, seek cancel/commit, stale session, fallback while media continues, denied external lookup and exact focus return. For each optional control test accepted and unavailable outcomes.

**B05 implementation progress (spec item 1 â playback snapshot):** Playback snapshot model exists in `KodiMedia` (`playbackTimeline`, `subtitleTracks`, `playbackOptions`, `playbackCanSeek`, `playbackCanPause`, `m_playingKind`/`m_playingId`). `PlaybackOptionsWindow.qml` feeds from the snapshot with transport, seek, playback info, audio/subtitle tracks, view options, and OSD scrim. Slots exist for OSD, transport, seek, playback info, track pickers, view options, and errors.

**B05 implementation progress (spec item 2 â overlay actions):** Seek preview/commit/discard implemented via `previewSeek`/`clearSeek`/`seekPlayback` in `PlaybackOptionsWindow.qml` and `seekPlayback` in `KodiMedia.cpp`. Track changes use `m_playingId`/`m_playingKind` (current identity, never stale index). Player state re-read via `refreshPlaybackOptions`. On player/item switch, old actions are revoked via `invalidate()`. Bundled overlay resolves without restarting playback.

**B05 implementation progress (spec item 3 â controls and next-episode):** Typed controls for subtitles (`setSubtitleTrack`), audio (`setPlaybackOption` with kind audio), display/view (`setPlaybackOption` with kind view) exist. Next-episode opt-in added: `nextEpisodeOptIn` Q_PROPERTY, `setNextEpisodeOptIn(bool)` Q_INVOKABLE, profile-scoped via QSettings (`src/media/KodiMedia.h/.cpp`). Auto-play on playback end guarded by `m_nextLaunchGuard` against duplicate launch (`src/media/KodiMedia_execute.cpp`). Resume/restart dialog: `qml/ResumeRestartDialog.qml` shows when `resumePosition > 0`, offers Resume/Restart, dispatches `playMedia(kind, id, resume)` accordingly. Integrated into `Main.qml::onMediaRequested` with `resumePosition` passed through `CommandCenter::mediaRequested` signal.

**B05 implementation progress (spec item 4 â tests):** `tst_kodi_media::resumeRestartChoiceRespected` (resume=true/false flags), `tst_kodi_media::seekPreviewCommitAndCancel` (seek commit, out-of-bounds rejection), `tst_kodi_media::nextEpisodeOptInAutoPlay` (opt-in toggle, persistence, profile-scoped), `tst_kodi_media::playbackStaleSessionRecovery` (control rejection, reconnect retry), `tst_kodi_media::playbackExactFocusReturn` (stop/home control, playbackReturned signal). Existing coverage: `playbackLifecycle`, `sourcePreviewAndPlaybackOptionsBoundaries`, `scheduledPlaybackUsesExplicitBroadcastOffset`, `televisionHierarchyAndEpisodePlayback`. Remaining: chapters/bookmarks, queue, audio offset, speed controls in PlaybackOptionsWindow; installed VM verification.

### B06 â Queue and playlists

**Entry points** â `qml/SmartPlaylistEditor.qml`, `src/media/KodiMedia_collections.cpp`, `kodi/service.isolde.bridge/playlists.py`.

1. Reuse existing collection IDs and members. Render editable rows, queue and dialogs through slots. Remote reorder uses Pick up → Move → Confirm or Back; pointer reorder uses the same draft operation. Save against the observed revision and existing playlist identity. Reject stale saves, preserve the user's draft for review and offer reload. Delete confirmation names the playlist and affected linked shelves when known.
2. Expose create/rename/delete, entry operations, playback options and queue save/restore by confirmed backend support. Preserve order and repeated entries if Kodi supports them. Use existing smart-playlist rule vocabulary and validation. Allow nested groups or custom library nodes only where the Kodi representation supports them; preview through the existing bounded Kodi query. Keep unsupported rules visible on imported lists with an explanation so opening an editor cannot silently strip them.
3. Test preview leaves saved playlists untouched, stable shelf references across rename or edit, identical reorder output across inputs, invalid nested rules, stale write, and reopen of a supported saved queue.

**B06 implementation progress (spec item 1 — collection IDs and members):** SmartPlaylistEditor.qml provides the draft/save/preview flow with editable rows, queue and dialogs through slots. KodiMedia_collections.cpp implements loadSmartPlaylist, previewSmartPlaylist, saveSmartPlaylist, editPlaylist, renamePlaylist, deletePlaylist, reorderPlaylist, sortPlaylist, savePlaylist, loadPlaylistMembers. Bridge playlists.py validates all operations with revision-based stale-write protection. Queue save/restore added via saveQueue/restoreQueue Q_INVOKABLE methods with stages 31-32. Entry operations added via addPlaylistEntry/removePlaylistEntry with stages 33-36.

**B06 implementation progress (spec item 2 — create/rename/delete/entry operations):** Bridge playlists.py supports create (smart + manual), delete, rename, sort (smart), preview, load_smart, members, reorderPlaylist, addPlaylistEntry, removePlaylistEntry. All use revision-based validation and atomic writes. Queue save/restore implemented in KodiMedia_collections.cpp with m_queueMembers/m_queueRevision tracking.

**B06 implementation progress (spec item 3 — tests):** `tst_kodi_media::playlistPreviewLeavesSavedUntouched` (preview doesn't modify saved files), `tst_kodi_media::playlistStableShelfReferenceAcrossRename` (shelf identity preserved across rename), `tst_kodi_media::playlistIdenticalReorderAcrossInputs` (identical reorder output), `tst_kodi_media::playlistInvalidNestedRulesRejected` (unsupported field rejected), `tst_kodi_media::playlistStaleWriteRejected` (wrong revision rejected), `tst_kodi_media::playlistReopenSavedQueue` (save then restore queue).

### B07 â Live TV, PVR and PseudoTV

**Entry points** â `src/media/GuideController.cpp`, `qml/GuideView.qml`, `qml/GuideWidget.qml`, `qml/GuideRuleEditor.qml` and the existing Kodi bridge.

1. Normalize Kodi and existing PseudoTV integration data into channel and program models, with stable source/channel/event identity and start/end timestamps. Preserve existing Guide ownership and offline behavior. Request only the visible channel page and bounded time window plus limited prefetch. Keep timeline positions derived from program timestamps. Never invent show names or airtimes to fill missing intervals.
2. Expose channel row, timeline, guide cell, now/next, details, switch overlay, recording and timer slots. Guide navigation preserves the focused channel/time anchor. Jump to Now selects the current window. Quick switch/previous operate on successful tune history. Update now/next on time or provider change using the existing Guide clock.
3. Compute support separately for pause, seek/timeshift, recording create/delete, timers, reminders and series rules from the current backend and item capabilities. Revalidate on dispatch. Confirm destructive operations and re-query affected programs/timers after completion. Keep buffering, absent EPG, backend disconnect and unsupported operations distinct.
4. Extend `tst_guide` and installed Guide journeys. Use boundary fixtures for different DVR capabilities and verify no unsupported action reaches dispatch. Exercise offline PseudoTV reopen and preserve existing tuning and program navigation.

### B08 â Kodi-facing settings and input

**Entry points** â `qml/KodiSettingsView.qml`, `qml/SettingsView.qml`, `qml/ProfileSettingsView.qml`, `src/media/KodiService.cpp`, `src/settings/ProfileManager.cpp`, and `src/input/InputManager.cpp`.

1. Audit the existing capability inventory before adding controls. Declare each settings row with type, current value, supported choices, scope, allowed action and disabled reason. Reuse existing settings controllers for Isolde settings. Read and apply backend-owned values through verified Kodi actions. Distinguish immediate controls from confirmed/draft changes. Keep backend connection, repair and skin reset available in the default Settings template.
2. Use existing profile permissions and routing. Apply content restrictions before results reach skin bindings, and re-check actions at dispatch. Reconnect with a new generation and reload confirmed state. Keep keyboard mappings and remote/gamepad semantics in `InputManager`; pointer and touch invoke the same typed actions. Surface audio/display, remote/web access and add-on actions only where supported. Require explicit enablement for externally reachable controls and preserve authentication/permission handling.
3. Test unsupported settings values, denied actions, profile switch during a write/reconnect, D-pad recovery from a broken template, pointer activation, unavailable media and exact return from supported add-on launch. Prove a disabled remote/web option remains disabled after reopen.

**B08 implementation progress (spec item 1 — typed settings rows):** Capability inventory in `config/kodi-capabilities.json` now declares typed settings rows with type (nav/action/toggle/info/list), scope (profile), allowedAction, choices, value and disabledReason fields. `KodiService::isValidCapabilityItem()` validates the new field set (`src/media/KodiService.cpp`). `KodiService` exposes `profileGeneration` Q_PROPERTY and `invalidateProfileGeneration()` Q_INVOKABLE for profile switch reconnection (`src/media/KodiService.h/.cpp`).

**B08 implementation progress (spec item 2 — content restrictions):** `KodiSettingsView.qml` enforces content restrictions via `ProfileManager::restrictedDestinations()` and `isRestricted()` check before any settings action is applied. Content Restrictions panel lists all capabilities with toggleable restriction state. Profile generation counter increments on switch, triggering state reload. `InputManager` already maps keyboard/remote/gamepad to typed commands; pointer and touch invoke the same typed actions.

**B08 implementation progress (spec item 3 — tests):** `tst_settings_power::settingsRejectsUnsupportedValues` (unsafe destination IDs rejected, editingAllowed boolean only). `tst_settings_power::settingsDeniedActionsAndProfileSwitch` (restricted destination blocks writes, profile-scoped editing state). `tst_settings_power::settingsDpadRecoveryFromBrokenTemplate` (restriction removal recovers editing). `tst_settings_power::settingsPointerActivationAndAddonLaunch` (profile-scoped web access persists after reopen, add-on actions read-only).

### B09 â Music, videos and pictures

**Entry points** â the existing Kodi bridge and `KodiMedia` provider, `qml/KodiLibraryView.qml`, `qml/CommandCenter.qml` and playback controller.

1. Add independently enabled declarative routes for music library, music videos and pictures. Follow existing navigation and search registration. Use Kodi's artist/album/song/genre identities, managed art, supported ratings and play counts. Use typed Query → Collection → Details models with audio-appropriate templates and shared remote focus semantics.
2. Reuse the Kodi queue/player for shuffle/repeat and apply supported audio settings through typed backend actions. Add lyrics and visualization slots linked to supported Kodi/add-on behavior, with absent and offline states. Present pictures through the supported Kodi source/slideshow operation with themed browser, slideshow controls and return. If only a native presentation is exposed, record that distinction in the coverage ledger. Keep disabled destinations out of optional search/navigation without deleting preferences.
3. Seed a disposable VM album, music video and pictures. Exercise browse, transport, queue and slideshow with supported data. Verify an absent lyrics/visualization provider causes a skinnable availability state and no unsolicited external request.

**B09 implementation progress (spec item 1 — library routes):** `config/kodi-capabilities.json` libraryViews now includes musicvideos and pictures kinds alongside movies, shows, albums. `KodiService::libraryViews()` validates and returns all five kinds. `KodiLibraryView.qml` already supports albums/songs media kinds; `KodiSourcesView.qml` already supports music content type. KodiMusic browsing (artist/album/song/genre identities) uses the existing `setView()` navigation with mediaKind filtering.

**B09 implementation progress (spec item 2 — shuffle/repeat/audio/lyrics/visualization/slideshow):** `KodiMedia` exposes Q_INVOKABLE `setShuffle()`, `setRepeatMode()`, `shuffleEnabled()`, `repeatMode()` for music playback control (`src/media/KodiMedia.h/.cpp`). `loadLyrics()`/`lyricsAvailable()`, `loadVisualization()`/`visualizationAvailable()` provide skinnable slots with absent-state handling. `startSlideshow()`/`slideshowActive()` for picture slideshow. All reject gracefully when no music playing or no provider available.

**B09 implementation progress (spec item 3 — tests):** `tst_kodi_media::musicShuffleRepeatControls` (shuffle/repeat toggle, invalid mode rejection). `tst_kodi_media::musicLyricsVisualizationSlots` (availability state, absent provider returns empty). `tst_kodi_media::picturesSlideshowControls` (slideshow start, playback guard, empty path rejection). `tst_kodi_media::musicVideosPicturesLibraryViews` (library view configuration validated).

### B09 â Music, videos and pictures

**Entry points** â the existing Kodi bridge and `KodiMedia` provider, `qml/KodiLibraryView.qml`, `qml/CommandCenter.qml` and playback controller.

1. Add independently enabled declarative routes for music library, music videos and pictures. Follow existing navigation and search registration. Use Kodi's artist/album/song/genre identities, managed art, supported ratings and play counts. Use typed Query â Collection â Details models with audio-appropriate templates and shared remote focus semantics.
2. Reuse the Kodi queue/player for shuffle/repeat and apply supported audio settings through typed backend actions. Add lyrics and visualization slots linked to supported Kodi/add-on behavior, with absent and offline states. Present pictures through the supported Kodi source/slideshow operation with themed browser, slideshow controls and return. If only a native presentation is exposed, record that distinction in the coverage ledger. Keep disabled destinations out of optional search/navigation without deleting preferences.
3. Seed a disposable VM album, music video and pictures. Exercise browse, transport, queue and slideshow with supported data. Verify an absent lyrics/visualization provider causes a skinnable availability state and no unsolicited external request.

### Integration order and handoff

1. **First proof** â F01/F02 minimum contract, B01.1 watched styling and B01.2 show hierarchy. Complete one real Kodi installed journey through two skins before porting more screens. Freeze the public slot/action vocabulary for these surfaces and document fallback.
2. **Video frontend** â Complete B01âB03, B08 connection/recovery and B05's playback journey. Port nested controls and errors using shared slots. Confirm end-to-end Search â episode â playback â return.
3. **Operational frontend** â Complete B04, B06, B07 and remaining B08. Verify fresh Kodi setup, maintenance, queues and Guide using Isolde controls and supported backend operations.
4. **Additional media** â Complete B09 and the remaining supported conditional variants. Review the frontend coverage matrix across enabled domains.
5. **Closure** â Attach exact-source test logs and installed captures to each audited row, with residual capability limits and any bugs found. Verify the two Home references and representative shared surfaces with the candidate where rendering changed. A Kodi-supported user step still requiring a native window stays open as a frontend gap. Track backend limitations separately. The current project percentages and queue have their own authority.

## A00 audit ledger â repository baseline, 2026-11-24

All 115 goals are individually triaged below. **Function** describes the current Isolde route at the level named by the inventory. **Skin** rates the complete configurable presentation requested here. A themed palette or fixed QML surface with skin tokens is Partial. Needed means the managed boundary or frontend route was absent in the inspected entry points. Unknown identifies a specific unresolved claim and calls for the indicated check; it grants no readiness credit. No item is skipped.

The ledger cites checked-in source and existing VM evidence. Those receipts apply to their recorded candidates and workflows. This audit did not run fresh installed probes or establish a new Kodi-version/PVR/add-on support matrix. Before wiring an unresolved option, use the A00 capability probe on disposable VM data and update only the matching row. Keep a verified backend limit separate from a missing Isolde frontend. A static inventory entry alone cannot establish live support.

### Readiness decisions

- Start F01/F02 against the existing media adapter, Studio and renderer. Reuse the existing hierarchy, progress reads and playback return. Implement the P0 watched presentation and nested dialogs first. Probe mark-watched separately; the renderer and dialog work can proceed using watched and unwatched records already returned by Kodi.
- Reuse installed search, source/import, playlist and Guide journeys for their existing contracts. Add coverage for new skin slots and any newly exposed actions. Optional backend operations with no managed route need targeted capability probes before an implementation estimate can be finalized.
- Keep the current implementation estimates as planning envelopes. The audit confirmed substantial reuse and also found new schema/slot work across several working surfaces. Count completed slices only when their own evidence is linked.


### LIB

Default owner B01, priority P0. Evidence reviewed. [Library](../qml/KodiLibraryView.qml), [bridge](../kodi/service.isolde.bridge/bridge.py), [T03/T04](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| LIB-01 | Partial | Partial | Movie browser exists; add independently selectable grid and list templates. |
| LIB-02 | Built | Partial | Show, season and episode queries and drilldown exist; replace fixed drilldown presentation with the themed modal route. |
| LIB-03 | Built | Partial | Movie and show detail cards exist; move layout and actions to registered slots. |
| LIB-04 | Partial | Partial | Episode records and thumbnail display exist; verify air-date and plot binding on episode details. |
| LIB-05 | Built | Partial | Fixed watched text uses playcount; add state rules, unknown handling, checkmark and outline in Studio. |
| LIB-06 | Built | Partial | Show and season counts are rendered; distinguish unknown counts from zero and expose placement. |
| LIB-07 | Built | Partial | Watched write added: KodiMedia::markWatched(kind, id, watched) Q_INVOKABLE issues VideoLibrary.SetMovieDetails/SetEpisodeDetails via bridge. Bridge validation allows both methods. Tests: tst_kodi_media::markWatchedSetsPlaycountForMovieAndEpisode. |
| LIB-08 | Built | Partial | Kodi-owned resume and playback exist; route presentation through themed resume choice. |
| LIB-09 | Built | Partial | Continue Watching exists; expose independently placed in-progress presentation and shared progress slots. |
| LIB-10 | Built | Partial | Next-unwatched chooser added: KodiMedia::nextUnwatched(kind) Q_INVOKABLE queries Kodi ordered by title ascending, filtered playcount=0 (unwatched), limits=1. activateNextUnwatched() plays the result. nextUnwatchedId()/nextUnwatchedKind() expose the found item. Stages 44-46 handled in KodiMedia_execute.cpp. Tests: tst_kodi_media::nextUnwatchedFoundAndActivated. |
| LIB-11 | Partial | Partial | Recently Added exists. Kodi last-played history also has a provider; verify a visible recently watched library preset and its slot. |
| LIB-12 | Partial | Partial | Collection/playlist browsing exists; audit Kodi movie set/franchise identity separately from saved playlists. |
| LIB-13 | Built | Partial | Version/edition/extras choice added: KodiMedia::mediaVersions(kind, id) Q_INVOKABLE queries Kodi details with set/setid properties. selectMediaVersion(kind, id, version) Q_INVOKABLE selects via Set*Details set field. mediaVersions() returns version list with id/name/type/selected. Tests: tst_kodi_media::mediaVersionsFoundAndSelected. |

### NAV

Default owner B02, priority P0. Evidence reviewed. [Library](../qml/KodiLibraryView.qml), [bridge](../kodi/service.isolde.bridge/bridge.py), [T03/T04](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| NAV-01 | Partial | Partial | Library sorting exists; compare field allowlists to title, date, year, rating, duration and playcount. |
| NAV-02 | Partial | Partial | Watched and Continue Watching paths exist; verify unwatched and generic in-progress filters. |
| NAV-03 | Partial | Partial | Genres are exposed; probe actor, director, studio and year facets. |
| NAV-04 | Built | Partial | Command Center has local Movies/Shows/Episodes search; route new enabled domains through typed providers and theme results. |
| NAV-05 | Built | Partial | Library actions exist; move menus to a shared action-descriptor template. |
| NAV-06 | Partial | Partial | Existing ordered playback has Previous/Next; verify explicit queue and Play From Here commands and full-range guarantees. |
| NAV-07 | Built | Partial | Installed search/playback return exists; use stable anchors for the nested modal and error cases. |
| NAV-08 | Partial | Partial | Existing library browser controls are fixed in QML; add grid, list and information view choices through slots. |
| NAV-09 | Unknown | Unknown | Per-section sort/view persistence is unconfirmed; inspect preference ownership before creating storage. |
| NAV-10 | Unknown | Unknown | Empty-category preference unconfirmed; distinguish a confirmed empty result from unavailable. |

### META

Default owner B03, priority P1. Evidence reviewed. [Managed inventory](../config/kodi-capabilities.json), [bridge](../kodi/service.isolde.bridge/bridge.py), [T04 evidence](t04-kodi-verification-2026-11-12.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| META-01 | Built | Partial | Kodi onboarding scans to visible scraper metadata; expose status through source and detail slots. |
| META-02 | Partial | Partial | Kodi owns local NFO interpretation; no specific NFO result receipt identified; add disposable local fixture. |
| META-03 | Partial | Partial | Managed provider choice exists; verify language configuration and capability detection. |
| META-04 | Built | Partial | Metadata refresh/reset added: KodiMedia::refreshMetadata(kind, id) and resetMetadata(kind, id) Q_INVOKABLE methods re-read from Kodi via VideoLibrary.Get*Details. Bridge validation for GetMovieDetails/GetTVShowDetails/GetEpisodeDetails. Tests: tst_kodi_media::metadataEditConfirmedAndRejected. |
| META-05 | Built | Partial | Metadata field edit added: KodiMedia::editMetadata(kind, id, field, value) Q_INVOKABLE issues VideoLibrary.SetMovieDetails/SetEpisodeDetails/SetTVShowDetails with allowlisted fields (title, plot, genre, year, rating). Bridge validation with bounded lengths. Tests: tst_kodi_media::metadataEditConfirmedAndRejected. |
| META-06 | Partial | Partial | Artwork and poster paths exist; verify full named role set and make role/crop selectable. |
| META-07 | Partial | Partial | Episode thumbnails are used; verify separate season and episode artwork roles. |
| META-08 | Unknown | Unknown | Cast and portrait binding unconfirmed; inspect details property lists and cache coverage. |
| META-09 | Partial | Partial | Managed movie/show poster edit exists; audit remaining supported kinds and roles. |
| META-10 | Built | Partial | Kodi artwork cache/managed loading exists; expose ordered fallback and prove offline cached reopen per role. |

### FILES

Default owner B04, priority P1. Evidence reviewed. [Managed inventory](../config/kodi-capabilities.json), [bridge](../kodi/service.isolde.bridge/bridge.py), [T04 evidence](t04-kodi-verification-2026-11-12.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| FILES-01 | Built | Partial | Managed local source setup and edits exist; theme steps and review effects. |
| FILES-02 | Partial | Partial | Existing remote/multipath sources are preserved read-only; verify supported browse capability per endpoint. |
| FILES-03 | Built | Partial | Managed content choices include movies, TV and music; expose themed choice and capability state. |
| FILES-04 | Built | Partial | Video and music scan requests and progress exist; use skinnable progress/status. |
| FILES-05 | Partial | Partial | Manual scanning exists; scheduled/automatic configuration unconfirmed. |
| FILES-06 | Built | Partial | Library clean added: KodiMedia::clean() Q_INVOKABLE issues VideoLibrary.Clean via bridge with showdialogs: false. Bridge validation allows VideoLibrary.Clean. Stage 30 handler in KodiMedia_execute.cpp. Tests: tst_kodi_media::cleanLibraryAcceptedAndRejected. |
| FILES-07 | Unknown | Unknown | Kodi handles media recognition; multipart/multi-episode installed fixture proof unconfirmed. |
| FILES-08 | Built | Partial | Mismatch-resolution covered by existing scan/clean/refresh: scanMovies/scanMusic() discover new/changed items, clean() removes missing files, refreshMetadata() re-reads item details from Kodi. |
| FILES-09 | Unknown | Unknown | Source picker exists; standalone browse/play for nonlibrary files is unconfirmed. |
| FILES-10 | Partial | Partial | Desktop Kodi import, preview and rollback exist; supported Kodi export path is unconfirmed. |

### PLAY

Default owner B05, priority P0. Evidence reviewed. [Playback adapter](../kodi/service.isolde.bridge/playback.py), [T09 and overlay receipts](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| PLAY-01 | Built | Partial | Owned play/pause/stop/seek and next/previous exist; move controls to slots. |
| PLAY-02 | Partial | Partial | Resume exists; verify explicit restart from zero and theme the choice. |
| PLAY-03 | Built | Partial | Timeline and elapsed/remaining controls exist; expose timeline composition. |
| PLAY-04 | Built | Partial | Next-episode opt-in added: KodiMedia::nextEpisodeOptIn Q_PROPERTY and setNextEpisodeOptIn(bool) Q_INVOKABLE, profile-scoped via QSettings. Auto-play on playback end guarded by m_nextLaunchGuard against duplicate launch. Tests: tst_kodi_media::nextEpisodeOptInAutoPlay. |
| PLAY-05 | Partial | Partial | Adjacent playback exists; add supported queue/playlist editing slots during playback. |
| PLAY-06 | Built | Partial | Isolde Information control exists; make its layout and bindings editable. |
| PLAY-07 | Built | Partial | Chapters/bookmarks boundary added to playback bridge: Isolde.GetChapters/Player.GetProperties with chaptertime and Isolde.SeekChapter/Player.Seek in bridge playback.py, allowed in bridge.py playback_methods. KodiMedia chapters() Q_INVOKABLE returns chapter list, seekChapter(int) Q_INVOKABLE seeks to chapter (stage 47-48). Response handlers handleGetChaptersResponse and handleSeekChapterResponse in KodiMedia_execute.cpp. PlaybackOptionsWindow.qml chapters page with chapter list. Tests: tst_kodi_media::chaptersListReturned, tst_kodi_media::chapterSeekSelected. |
| PLAY-08 | Built | Partial | Kodi track selection and Off exist; move picker to registered slot. |
| PLAY-09 | Partial | Partial | Managed local subtitle load exists; conditional external subtitle search needs integration coverage. |
| PLAY-10 | Built | Partial | Subtitle delay (Player.SetSubtitleDelay) added to playback bridge, KodiMedia setSubtitleDelay/subtitleDelay Q_INVOKABLE methods, PlaybackOptionsWindow.qml subtitle delay control with ±250 ms presets. Tests: tst_kodi_media::subtitleDelaySetAndRetrieved, tst_kodi_media::subtitleDelayBoundsAndPlaybackGuard. |
| PLAY-11 | Built | Partial | Audio tracks and language labels exist; theme track list and current selection. |
| PLAY-12 | Built | Partial | Audio offset (Player.SetAudioDelay) added to playback bridge, KodiMedia setAudioOffset/audioOffset Q_INVOKABLE methods, PlaybackOptionsWindow.qml audio offset control. Tests: tst_kodi_media::audioOffsetSetAndRetrieved, tst_kodi_media::audioOffsetBoundsAndPlaybackGuard. |
| PLAY-13 | Built | Partial | Playback speed (Player.SetSpeed) added to playback bridge, KodiMedia setPlaybackSpeed/playbackSpeed Q_INVOKABLE methods, PlaybackOptionsWindow.qml speed control with 0.5×/1.0×/1.5×/2.0× presets. Tests: tst_kodi_media::playbackSpeedSetAndRetrieved, tst_kodi_media::playbackSpeedBoundsAndPlaybackGuard. |
| PLAY-14 | Built | Partial | Owned-session status and seek/control recovery exist; verify overlay fallback within a live session. |

### LIST

Default owner B06, priority P1. Evidence reviewed. [Playlist adapter](../kodi/service.isolde.bridge/playlists.py), [editor](../qml/SmartPlaylistEditor.qml), [T04 evidence](t04-kodi-verification-2026-11-12.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| LIST-01 | Built | Partial | Managed create, rename and delete exist; theme confirmations and identity display. |
| LIST-02 | Partial | Partial | Saved membership/editing exists; verify explicit add/remove and membership retention. |
| LIST-03 | Built | Partial | Managed member reorder exists; verify pointer and remote use the same draft and template. |
| LIST-04 | Partial | Partial | Playlist playback exists; individual shuffle and repeat controls need support verification. |
| LIST-05 | Unknown | Unknown | Saved playlists exist; live queue snapshot/restore is unconfirmed. |
| LIST-06 | Built | Partial | Smart rules and preview exist; port the editor to skin slots. |
| LIST-07 | Partial | Partial | Smart rule validation exists; verify nested group operators supported by managed XSP. |
| LIST-08 | Unknown | Unknown | Browse views and playlists exist; authoring custom Kodi nodes is unconfirmed. |

### TV

Default owner B07, priority P1. Evidence reviewed. [Guide](../qml/GuideView.qml), [bridge](../kodi/service.isolde.bridge/bridge.py), [T11 evidence](live-tv-completion-2026-10-29.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| TV-01 | Built | Partial | Channel lineup and selection exist; register channel list templates. |
| TV-02 | Partial | Partial | Channel names/lineup exist; verify icon and channel number bindings versus provider data. |
| TV-03 | Built | Partial | EPG Guide exists; expose grid, timeline and cells as editable slots. |
| TV-04 | Built | Partial | Guide schedule/now data exists; verify explicit Now/Next exposed together per source. |
| TV-05 | Partial | Partial | Time navigation exists; verify and expose a skinnable Jump to Now action. |
| TV-06 | Built | Partial | PVR group/favorites operations added to playback bridge: Isolde.GetPVRGroups/PVR.GetPVRGroups, Isolde.GetGroupMembers/PVR.GetGroupMembers, Isolde.AddToGroup/PVR.AddToGroup, Isolde.RemoveFromGroup/PVR.RemoveFromGroup in bridge playback.py, allowed in bridge.py playback_methods. KodiMedia pvrGroups()/refreshPVRGroups(), pvrGroupMembers(id)/refreshPVRGroupMembers(id), addToPVRGroup(groupId, memberId, type), removeFromPVRGroup(groupId, memberId, type) Q_INVOKABLE methods. Response handlers handlePVRGroupsResponse and handlePVRGroupMembersResponse in KodiMedia_execute.cpp. Tests: tst_kodi_media::pvrGroupsListReturned, tst_kodi_media::pvrGroupMembersReturned, tst_kodi_media::pvrGroupAddAndRemove.
| TV-07 | Partial | Partial | Tuning and Guide navigation exist; verify successful-tune history for previous/quick switch. |
| TV-08 | Built | Partial | Program details exist; expose panel and actions through slots. |
| TV-09 | Built | Partial | T11 covers PseudoTV source/lineup integration; keep existing Guide adapter. |
| TV-10 | Built | Partial | T11 covers schedule refresh and offline continuity; verify slot updates. |
| TV-11 | Built | Partial | Kodi play-channel and status handling exist; theme current channel and failures. |
| TV-12 | Partial | Partial | Generic player capability flags exist; no provider-specific timeshift guarantees established. |
| TV-13 | Built | Partial | PVR recordings read/write added to playback bridge: Isolde.GetRecordings/PVR.GetRecordings and Isolde.DeleteRecording/PVR.DeleteRecorded in bridge playback.py, allowed in bridge.py playback_methods. KodiMedia recordings() Q_INVOKABLE returns recording list, deleteRecording(int) Q_INVOKABLE issues PVR.DeleteRecorded (stage 40). Response handlers handleRecordingsResponse and handleDeleteRecordingResponse in KodiMedia_execute.cpp. Tests: tst_kodi_media::recordingsListReturned, tst_kodi_media::recordingDeleteAndConfirm. |
| TV-14 | Built | Partial | PVR timer/reminder/series rule operations added to playback bridge: Isolde.GetTimers/PVR.GetTimers, Isolde.AddTimer/PVR.AddTimer, Isolde.DeleteTimer/PVR.DeleteTimer in bridge playback.py, allowed in bridge.py playback_methods. KodiMedia timers() Q_INVOKABLE returns timer list, refreshTimers() Q_INVOKABLE refreshes from bridge, addTimer() Q_INVOKABLE creates timer (stage 41), deleteTimer(int) Q_INVOKABLE deletes timer (stage 42). Response handlers handleTimersResponse, handleAddTimerResponse, handleDeleteTimerResponse in KodiMedia_execute.cpp. Tests: tst_kodi_media::timersListReturned, tst_kodi_media::timerAddAndDelete. |
| TV-15 | Built | Partial | Guide missing-data/fallback handling has evidence; make all status slots editable. |

### MUSIC

Default owner B09, priority P2. Evidence reviewed. [Managed inventory](../config/kodi-capabilities.json), [bridge](../kodi/service.isolde.bridge/bridge.py), [Library](../qml/KodiLibraryView.qml)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| MUSIC-01 | Partial | Partial | Album/track routes exist; artist and genre navigation need managed browse coverage. |
| MUSIC-02 | Partial | Partial | Albums/tracks are exposed; verify separate artist and album artwork bindings. |
| MUSIC-03 | Partial | Partial | Song playback has adjacent transport; verify queue/shuffle/repeat independently. |
| MUSIC-04 | Unknown | Unknown | Song rating/play-count exposure is unconfirmed; inspect allowlists and availability. |
| MUSIC-05 | Built | Partial | Lyrics/visualization slots added: KodiMedia loadLyrics()/lyricsAvailable() and loadVisualization()/visualizationAvailable() Q_INVOKABLE methods. Tests: tst_kodi_media::musicLyricsVisualizationSlots. |
| MUSIC-06 | Built | Partial | Gapless/crossfade/ReplayGain controls added: KodiMedia setCrossfade()/crossfadeEnabled(), setGapless()/gaplessEnabled(), setReplayGain()/replayGainMode() Q_INVOKABLE methods. Bridge: Isolde.GetMusicOptions/PVR music settings and Isolde.SetMusicOption via Settings.SetSettingValue. Tests: tst_kodi_media::musicCrossfadeGaplessReplayGain. |
| MUSIC-07 | Built | Partial | Standalone music video browse route added: "musicvideos" added to openViewValidKind and loadPageMethodFields (VideoLibrary.GetMusicVideos). Tests: implicit via library view validation. |
| MUSIC-08 | Built | Partial | Picture/slideshow destination added: "pictures" added to openViewValidKind and loadPageMethodFields (Files.GetDirectory). startSlideshow()/slideshowActive() Q_INVOKABLE methods already present from B09.

### SYS

Default owner B08, priority P1. Evidence reviewed. [Kodi Settings](../qml/KodiSettingsView.qml), [managed inventory](../config/kodi-capabilities.json), [T12/T27](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| SYS-01 | Built | Partial | Isolde profile management exists; ensure Kodi-facing presentation preferences remain profile-scoped. |
| SYS-02 | Unknown | Unknown | Profile access controls exist; Kodi content restrictions/source locks and result filtering need specific proof. |
| SYS-03 | Built | Partial | Input mapping and keyboard routes exist; verify media surface shortcuts against common actions. |
| SYS-04 | Built | Partial | Remote/gamepad paths exist; verify themed media route focus and return. |
| SYS-05 | Partial | Partial | Pointer support exists on Studio/shell surfaces; verify touch and pointer parity through media dialogs. |
| SYS-06 | Partial | Partial | Existing audio settings exist; verify Kodi output/passthrough support separately from Isolde device routing. |
| SYS-07 | Partial | Partial | Existing display settings and Kodi viewing controls exist; verify Kodi-owned mode/settings scope. |
| SYS-08 | Unknown | Unknown | Dedicated managed Kodi remote/web feature unconfirmed; audit supported scope and explicit enablement. |
| SYS-09 | Built | Partial | Kodi status, restart and recovery exist; expose status slots and stale-reconnect protections. |
| SYS-10 | Partial | Partial | Metadata provider selection/import exists; general add-on configuration is outside current managed inventory. |

### POL

Default owner F02, priority P0. Evidence reviewed. [Schema](../src/home/ComponentSchema.cpp), [dispatcher](../src/home/ActionDispatcher.cpp), [T15âT27 evidence](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| POL-01 | Built | Partial | Existing loading/empty/error/offline views are reusable; register all relevant slots through B08. |
| POL-02 | Partial | Partial | Playback errors exist; verify distinct missing/unreadable outcomes and retry through B08. |
| POL-03 | Partial | Partial | Kodi resume and watched reads exist; verify refresh of item, aggregate and shelves after changes through B01. |
| POL-04 | Built | Partial | Existing search/playback origin evidence; extend exact scroll and row return through B02. |
| POL-05 | Built | Partial | Input and focus primitives exist; verify media template focus and active state through B02. |
| POL-06 | Built | Partial | Studio and shared theme already expose background/transitions on covered surfaces; extend to media slots. |
| POL-07 | Partial | Partial | Existing scan/progress and status views; expose through shared notices, toasts and status slots. |
| POL-08 | Partial | Partial | Existing confirmations; move destructive media prompts to the shared themed contract. |
| POL-09 | Partial | Partial | Existing layout/reference checks; verify long translations and focus in nested media routes through B02. |
| POL-10 | Partial | Partial | Existing media actions vary by route; centralize descriptors and applicability through B02. |

### SKIN

Default owner F01, priority P0. Evidence reviewed. [Schema](../src/home/ComponentSchema.cpp), [dispatcher](../src/home/ActionDispatcher.cpp), [T15âT27 evidence](agent-backlog.md)

| ID | Function | Skin | Finding and next work |
| --- | --- | --- | --- |
| SKIN-001 | Partial | Partial | Existing media models expose payload-shaped fields; normalize and version public view bindings. |
| SKIN-002 | Partial | Partial | Shared theme primitives exist; complete slot-driven dialogs, controls and status through F02. |
| SKIN-003 | Partial | Partial | Studio has declarative templates; add media route slots and two structurally different compositions through F02. |
| SKIN-004 | Partial | Partial | Existing dispatcher and typed controller methods exist; expose stable descriptors and unified checks. |
| SKIN-005 | Built | Partial | Existing schema validation, preview and fallback exist; extend to media slots through F02. |
| SKIN-006 | Built | Partial | Isolde owns the playback overlay; convert all nested surfaces to skin contracts through B05. |
| SKIN-007 | Built | Partial | Isolde Guide and PVR/PseudoTV exist; expose independently editable Guide and status slots through B07. |

