# Isolde Studio — Home Screen Studio and universal grid builder

## Page Studio replacement — user decision, 2026-10-19

The user rejected the current Customize Home workflow and requested a new intuitive visual page builder inspired by web design tools. [page-studio-redesign.md](page-studio-redesign.md) is the normative interaction, workspace, transaction and acceptance specification. This is a native Qt canvas-first system: visual grid/ghost placement, remote pick-up/drop, contextual inspector, Pages/Layers, shared semantic commands, draft/undo before manipulation, and explicit Preview/Apply. Extend existing schemas/models/adapters; do not bolt another list of actions onto the rejected editor or introduce a second layout store.

The backlog replaces T06-T08 with T15-T24 and excludes the superseded rows from percentages. Selected priority is T15 → T16 → T17 → T18 → T19 → T20 → T21 → T22 → T23 → T24. This overrides older curation-first, immediate-save and numbered-package sequencing below. Unfinished T01/T03/T04/product gates stay active, but do not block this explicitly selected editor track; existing verified foundations are reused. T17 supplies T05's unfinished placement-geometry evidence. T09/T10/T11 now depend on completed Studio T24; T10 hardware qualification remains mandatory for actual device editors. Product release still requires its full backlog and hardware/offline gates.

This chat authorizes plan/backlog changes only. No new editor code, VM deployment or dependency change is part of this revision. The existing verified build stays the implementation baseline until future tasks meet their acceptance gates.

> Current authority (2026-10-19 audit): [agent-backlog.md](agent-backlog.md) selects the next task and records completion; this document supplies requirements/dependency rationale. Older dated next-action and not-started statements below are historical. T02 curation extensions are out of scope; existing movie/show collections remain unchanged. Source presence is not acceptance. See [planning audit](planning-audit.md) and [VM oracle](vm-oracle.md).

Incorporated October 17, 2026 from the user-supplied **Isolde Studio** document (Version 5, 17 October 2026; three parts: the Home Screen Studio plan (sections 1–11), the Universal Interface Builder expansion (sections 12–22), and the build-ready implementation specification (sections 23–41)). The original attachment remains the source design reference; this document consolidates it for the repository. Everything below is a requirement, not a claim of implemented capability. The current milestone remains **0.0.1.2**; sequencing is governed by [implementation-order.md](implementation-order.md).

The Studio supersedes home-screen-only assumptions in older planning: the same editing system must eventually cover Home, destination pages, collections, search, settings, device control and future surfaces. It extends, and does not replace, the product contracts in [master-roadmap.md](master-roadmap.md): defaults stay safe, nothing requires customization, existing layouts are never rewritten on startup, Kodi retains media ownership, and the Generator Test is unchanged.

## Non-negotiables

The Studio must support desktop and TV; remote-first interaction with complete keyboard/mouse parity; layout stored as validated data, never executable QML/JavaScript; every editor control bound to a real backend capability; every widget previewable and validatable; no parallel media database (Kodi remains the media source of truth); profile safety (edits affect only the active profile); and offline-first editing, preview and applying.

## Universal grid

Every screen uses a configurable two-dimensional grid rather than hardcoded rows. Each widget occupies one or more rectangular cells; grid guides appear only in edit mode.

| Property | Proposed starting behavior |
| --- | --- |
| Columns | 4–16; default 8 |
| Rows | 3–10; default 5 |
| Widget width / height | 1 to the total column/row count |
| Gaps, margins | Adjustable; outer margins respect the device safe area |
| Snap | Always snap widget bounds to cells |
| Overlap | Disabled by default |
| Overflow | Warn and prevent applying an invalid configuration |
| Empty cells | Allowed; intentional negative space is valid |
| Visibility | Grid guides visible only in the editor |

Widget position model: `X` (start column), `Y` (start row), `width` (column span), `height` (row span), with display styling (padding, background, radius, opacity, artwork, typography) as separate properties. Layouts scale proportionally between resolutions, particularly across 16:9 televisions. Collisions show occupied cells and offer shift, swap or cancel choices; the Studio never silently overwrites placement. A larger widget may host an internal grid (for example a 4 × 2 HDMI switcher with eight subordinate input buttons) that is edited separately from the screen grid.

## Editing modes, workspaces and parity

Three progressive modes: Simple (apply a preset), Studio (grid, components, themes), Advanced (nested screens, variables, triggers, visibility rules). Five workspaces all editing the same underlying layout data: Visual Studio, Component Studio, Data Studio, Interaction Studio, Theme Studio.

TV editing is remote-first: select a widget, open its options menu (Move / Resize / Customize / Duplicate / Remove), navigate with the D-pad, enter spans numerically, and resolve collisions from a choice dialog. Desktop editing adds drag-and-drop, a properties inspector, keyboard shortcuts and canvas zoom. Undo/redo preserves an editable history. Preview mode runs the real renderer without saving; Apply validates the draft, writes atomically, and rolls back on failure. Both editing surfaces must produce identical layout data, and every grid control must remain actionable with directional keys.

## Component model

Three contracts per component: widget (data and properties), renderer (visual presentation), studio adapter (editor controls generated from the schema). Component families: media (movie/series/episode/music/game/app/collection cards and shelves), navigation (bars, tabs, docks, breadcrumbs, sidebars, jump lists, back buttons, search shortcuts), control (buttons, inputs, toggles, sliders, pickers, forms, device controls), device (input grid, HDMI switcher, audio routes, device status), content (text, image, custom widgets, web surfaces), utility (clock, date, weather, status, calendar, reminders, notifications, profile switcher, quick settings, scene/mood, featured content, ambient mode). Components declare supported properties so the Studio generates their controls; they declare actions, events, tokens and validation so failures stay isolated and repairable.

Media-backed widgets bind to Isolde collections through the existing Kodi adapters; the Studio does not create a second media library. Device widgets bind through the existing capture and audio adapters and present unavailable states honestly until those backends are qualified. Weather remains a permission-aware widget behind a provider interface with explicit offline behavior.

## Pages, containers and responsive rules

Screen types: Home, Destination, Collection, Search, Settings, Device Control, Profile, Media Detail, External, Custom. Containers: grid, flex/flow, stack, carousel, masonry, dock, sidebar, tabs, nested screen, overlay/modal. Breakpoints: portrait below 600, compact 600–960, medium 960–1280, TV 1280 and above, ultra 2160 and above. Containers manage their own spacing, alignment, wrapping and clipping; navigation preserves focus and scroll across playback and overlays; Settings and Home recovery remain reachable from every surface.

## Actions, state and variables

Built-in actions: Navigate, Back, Home, OpenModal, CloseModal, SetVariable, ToggleVariable, ApplyFilter, SelectItem, ScrollTo, FocusElement, PlayMedia, ResumeMedia, AddToQueue, LaunchApp, SwitchHdmi, ToggleFullscreen, OpenSearch, RefreshQuery, ShowToast, SetTheme, StartTransition. Actions are typed and versioned; sequences support condition checks and timeout/error branches, and async actions expose loading/success/failure. Variables support booleans, numbers, strings, enumerations, selected media IDs and filter structs, scoped to component, page, session or persistent profile. Guards prevent recursive navigation/action loops. Privileged functionality is exposed only through explicit adapters and permissions — never raw shell execution. Button-to-page linking is a no-code, previewable, validation-checked workflow.

## Backgrounds, motion and accessibility

Every page and eligible container supports layered backgrounds: solid, gradient, image, animated image (GIF) or video when decoders exist. Requirements: local managed assets only; fit modes cover/contain/stretch/tile with focal point; autoplay looping on visible with offscreen pause; bounded asset size, decoded dimensions, frame count and decode memory; asynchronous decode with bounded frame caches; reduce-motion falls back to a still poster or first frame; missing or unsupported assets fall back to a readable static surface, never a blank page. Video backgrounds may follow static/GIF support behind an explicit decoder abstraction; never promise universal codec support. Motion categories with editable properties: page transition, focus transition, container entrance, carousel, menu animation, wallpaper transition, modal transition and an optional bounded timeline. Accessibility requires a visible focus indicator, high-contrast presets, readable text at viewing distance, text scale, reduced motion, photosensitivity guards, and accessible focus order, labels and hints.

## Templates, tokens and themes

Preset templates: source dashboard, movie library, series library, continue watching, music dashboard, game library, device control, profile landing, custom dashboard, scene launcher. Templates create new layouts or replace content in preview without destructive application. Symbols are reusable component instances with global or local overrides. Tokens cover color, typography, spacing, radius, effects, motion and component-specific values; the token editor previews usage and impact before applying. Import validates and previews every time; only explicitly permitted extension metadata may be preserved inertly; unknown versions, component/action types and unsafe actions are rejected at the boundary.

## Canonical layout schema

Persist versioned JSON with `schemaVersion`, layout identity/description, breakpoints, columns, rows, grid gap, safe area, tokens, pages, overlays, themes, variables, export metadata and modification time. Each component stores `id`, `type`, `name`, position (`x`, `y`, `width`, `height`), layout (`container`, `span`, `alignment`), `presentation`, `content` (`collectionId`, `query`, `limit`, `sort`, `fallback`), `actions`, `state`, `visibility`, `accessibility` and `validation`. Migration never overwrites without a backup; this is consistent with the established `home-layout.json` version-1 read compatibility and version-2 atomic writes with original backup (see [home-composition.md](home-composition.md)).

## Relationship to the current implementation

The plan's repository-audit step (M0) cites `profile-layout.{h,cpp}` and `layout-repository.{h,cpp}`; this repository does not contain those files. The current equivalents are `src/home/HomeLayout.{h,cpp}` (the layout authority with versioned load/save), `src/home/ComponentSchema.{h,cpp}` (validated actions and presentation), `src/home/HomePages.cpp`, `src/home/HomeSkins.cpp`, `src/home/PageData.{h,cpp}` and `src/home/ActionDispatcher.{h,cpp}`. Current editing surfaces are Customize Home, Component layout editing, Design Home, Home skins and the page composer ([pages.md](pages.md)); the Studio's Simple/Studio/Advanced modes must extend these surfaces and preserve their recovery guarantees — Settings and Edit Home reachable at all times, and a layout with every launcher removed must still have a recovery route.

Both standing visual fixtures remain pass gates for every Studio slice: [home reference](../tests/visual/home-reference/README.md) and [pastel reference](../tests/visual/home-pastel-reference/README.md). Studio preview/apply/rollback must keep both recreation/reset pairs passing, and the reference configurations must never become special-cased screens.

## Build sequence and acceptance

The plan proposes build modules M0–M8: repository audit; canonical layout schema with validation/migration tests; Studio shell with preview/apply/rollback; grid editor (move, resize, span, collisions, undo/redo); typed components with contract tests; pages, containers, motion and backgrounds; collections, device adapters and templates/tokens; capture input editor, HDMI switcher editor, audio routes editor and keyboard/remote integration; performance, hardening and migration tests. Each step must land independently, keep existing layouts compatible and keep reference previews passing. Device editors depend on capture/audio backend qualification and must not claim hardware readiness from software presence.

M1 progress (October 17): the grid data contract is implemented — validated `composition.grid` and per-widget `placement`, no-strand grid changes, collision reporting, atomic saves with rollback, and fallback-on-read for invalid stored placements, with regression coverage in the component suite. The grid editor and Studio shell (M2–M3) are not started and the renderer contains placement handling, but current-source fixture acceptance is unverified; see [home-composition.md](home-composition.md) for the field contract.

Acceptance highlights: the Studio opens the active profile layout; preview reflects the layout before applying; grid editing supports move, resize and span; every required widget is present and configurable; collection-backed widgets show real or clearly labeled synthetic data; device widgets reflect real availability; tokens and themes apply consistently; templates create layouts without destructive replacement; actions and variables edit and execute; import/export round-trips portable projects; keyboard and remote complete every workflow; profile isolation, offline operation and schema migration are verified.

Version note: the plan proposes a baseline Studio release at 0.0.1.2 and a full Studio at 0.0.1.3. This repository has not adopted that numbering: 0.0.1.2 remains the current backend and capture milestone with open qualification work, and Studio version decisions belong to [implementation-order.md](implementation-order.md) when a slice is scheduled.

Out of scope per the plan: external CDN assets, remote loading of code or themes, scraping, an app store, a home-automation hub, raw Kodi library writes, forced upgrades, and executable QML/JavaScript inside layouts. Recorded risks are scope growth, rendering performance on low-power television hardware, device-editor hardware dependence and schema-migration safety; the mitigations are bounded validation, resource caps, isolated component failures, offline tests and the reference fixtures above.
