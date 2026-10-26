# T11 — Unified Live TV proposal review

Status: recommended design amendment, 2026-10-26. Review only: no application changes, VM inspection, provider-account verification or acceptance credit. Input: user attachment `Isolde_Unified_Guide.txt`, T11 entry docs/files and master-plan Live TV Engine sections 1–12. Existing requirements remain included; T11's T24 dependency is unchanged.

## Assessment

Keep the hybrid approach: Isolde owns guide presentation, personal lineups and its native local-channel schedules; Kodi owns media, playback and existing PVR/PseudoTV facilities; external providers supply only capabilities their interfaces support.

The proposal is effective for aggregation but incomplete for local scheduling. Its tables omit rules, immutable schedule slots, broadcast clocks and generation state. Its phases postpone local channels too far, and its `native_tune` category conflates playback ownership, precise app handoff and EPG. Correct those points before implementation. A tunable channel without listings is valid; listings do not prove tuning support.

Narrow repository inspection found existing Kodi media/artwork/import/capture services, no guide store under `src/media/`, and library/playback rather than PVR methods in the inspected `KodiMedia.h`. The backlog records static guide summaries rather than an implemented EPG. This does not establish which sources are configured in the VM; inspect those at implementation entry through the bounded runner.

## Source feasibility

| Source | Recommended approach | Unproven capability |
| --- | --- | --- |
| Isolde local channels | Persist native schedules, resolve Kodi IDs, play/seek through Kodi | Scheduler, actual elapsed seek, transitions and disconnected acceptance |
| Kodi PVR/existing PseudoTV | Read groups/channels/broadcasts through the supported bridge; retain original definitions and schedule ownership | Installed API/backend exposure, stable identities, tune and return |
| IPTV/XMLTV/tuners | Prefer configured Kodi backends; explicit setup and mapping for new sources | Real feed/backend, malformed-input behavior, no-EPG playback |
| Plex server tuner/DVR | Separate connector from Plex free TV; investigate supported interfaces | No assumed Plex-to-Kodi bridge; discovery, entitlement, EPG and tuning |
| Plex free Live TV | App destination initially; channel integration only with supported interface evidence | Catalog, EPG and exact-channel handoff |
| YouTube TV | Source/app destination initially; promote capabilities independently | Supported channel catalog/EPG interface, regional entitlement, precise tune and remote return |

Plex documents its free channels separately from tuner/DVR and says free channels cannot use its DVR. Google documents supported YouTube TV viewing devices. Google's YouTube Live Streaming API manages creator broadcasts; it is not a subscription-TV guide API. No supported public YouTube TV catalog/EPG interface was established in this review. That is a feasibility finding, not proof that partner integration is impossible. SmartTube/ordinary YouTube support does not establish YouTube TV support.

Official references checked 2026-10-26:

- [Kodi JSON-RPC](https://kodi.wiki/view/JSON-RPC_API/v13): use actual installed-version introspection rather than assume v13.
- [Plex free TV FAQ](https://support.plex.tv/articles/faq-live-tv-on-plex/): search-index excerpt available; direct fetch returned 403.
- [Plex tuner/DVR setup](https://support.plex.tv/articles/225877347-live-tv-dvr/).
- [YouTube TV supported devices](https://support.google.com/youtubetv/answer/7129767?hl=en).
- [YouTube Live Streaming API scope](https://developers.google.com/youtube/data/live/getting-started).

## Improved interface

Use the shared appearance/composition model. The following describes a configurable default preset, not fixed QML navigation, colors or categories.

```text
Live TV                     Search   Now   Add   Manage
All channels   Favorites   [user groups]   Sources
Selected program: title / time / source / availability
Channel                 19:00        19:30        20:00
[number / name]         [current program] [next program]
[number / name]         [No listings — Watch live]
Menu: Options                            Back: Previous
```

Keep channel labels fixed; virtualize visible rows and timeline windows. Now restores current time while retaining the channel. Groups and source filters are distinct; do not hard-code Sports/Local/Plex chips. Show source identity in details/accessibility even when badges are visually hidden. Display stale/offline/missing listings accurately.

App-only services live in a reachable Services panel under Sources, not fabricated broadcast rows. Real EPG with app-only playback may occupy guide rows labeled “Open in [app]”, explaining additional navigation. Manual channel shortcuts remain explicitly manual, with no invented EPG.

Add offers equal choices:

1. **Create local channel:** media/saved rules → name/number → nested rule editor and match preview → clock/sequencing preview → Save channel. Default choices keep this short; advanced blocks remain available. Cartoon Network must be created in one save without opening Kodi's editors. Failed save preserves the draft.
2. **Add source:** existing Kodi channels, IPTV/XMLTV, tuner/backend, supported service or compatible direct stream. Preview discovered capabilities before import. For app-only providers say “Opens the app; channels cannot currently be imported.” Installed does not mean authenticated or entitled. Local channels need no account.

Manage offers Channels, Groups and Sources: hidden channels, favorites, names/logos/numbers, remote ordering, source health/retry and explicit removal consequences. Remove imported rows must not delete Kodi configuration or media. Retain dormant preferences for returning sources. Number collisions offer swap/choose another/cancel. Reordering previews a complete draft; Confirm applies, Back restores. Desktop drag and remote controls serialize identically.

Confirm on a current directly tunable program watches it. Future programs open details rather than accidentally tuning whatever is currently on. Handoff actions identify the provider. Expose recording, restart, pause and catch-up only when actually supported. Menu supplies every contextual action; long press is optional.

## Remote and return behavior

- Up/down retains a preferred timeline instant; left/right walks programs/time windows. Gaps remain selectable. Up from the first row reaches controls; empty/error states retain Add, Manage, Settings/recovery and Back.
- Provide date/time jump and Now using ordinary D-pad/Confirm/Back/Menu controls. No workflow may require number keys, pointer input or a dedicated Guide button.
- Before launch retain profile/page/widget, channel ID, program/time anchor, filters and scroll. Restore exactly, with a visible nearest-channel fallback when the original disappears.
- Back from Isolde-owned playback returns to the guide. Stop is separate; background playback follows the existing configurable policy. Channel surfing uses visible order where Isolde owns those commands.
- External apps may consume Back. Reuse session/input coordination and verify an escape using the minimum supported controls; a dedicated Guide/Home key is supplementary. Do not globally intercept Back during login/text entry. Failed launches and crashes restore origin without relaunch loops. Unsupported return interception is an explicit platform limitation, not a claimed pass.

## Data and scheduling corrections

Use one profile-scoped SQLite WAL guide store shared by widgets, overlay and full guide. Reference Kodi IDs and managed artwork; retain only bounded metadata snapshots needed for rule evaluation and guide display. Do not build a second media database, scraper or artwork acquisition service.

Retain proposed provider instances, channels, preferences, groups, program cache and targets. Add:

| Entity | Required information |
| --- | --- |
| ChannelDefinition | Schedule owner, rule reference, enabled state and revision |
| ChannelRules | Versioned nested predicates, exclusions, sequencing, blocks/cooldowns/filler |
| ChannelClock | Clock mode, timezone, epoch/cursor, last observed time and clock discontinuity |
| ScheduleSlot | Immutable ID, channel/media reference, UTC [start,end), generation revision, repair status |
| GenerationState | Seed, deterministic ordering/PRNG state, candidate revision, cursor and generated-through instant |
| SourceSyncState | Instance identity, mapping revision, freshness, retry/backoff, successful refresh generation |
| GuideReturnState | Profile/page/widget, channel/time, filters, scroll and session generation |

Clock advancement and sequencing are independent: real-time/pause-when-off describes time; shuffle/episode order/rotation describes content. Fixed blocks need timezone and DST ambiguity policy. Define “off”: recommended pause boundary is profile deactivation or Isolde exit, not leaving the guide. Persist checkpoints and specify bounded crash recovery.

Channel identities include provider instance/account/backend identity. Kodi numeric IDs alone may change after backend recreation; ambiguous remapping needs confirmation. Never merge channels by name. Optional preferred-source groups retain separate schedules and provenance; fallback must not silently substitute another broadcast.

Replace the single tune-mode ladder with independent catalog, EPG, launch kind, playback owner, seek/pause/record, verified return and runtime availability fields. Supported channel links and app-home launches differ. Unknown is a valid capability state. Credentials are secure-store references and excluded from logs/exports.

One channel has one scheduler: preserve imported PseudoTV schedules; generate only Isolde-native channels. No PVR export bridge is required initially: tune the native slot through Kodi. Append a default seven-day horizon off the UI thread; opening the guide only reads a cached window. Persist deterministic generation state with slots. Rules affect uncommitted work; Regenerate previews the replacement boundary and requires explicit acceptance. Missing-media repair preserves unaffected IDs/times and handles differing replacement durations explicitly.

Validate positive, non-overlapping UTC [start,end) intervals transactionally. A unique start-time constraint cannot prevent arbitrary overlap in SQLite. Serialize per-channel generation and compare revisions. Save channel, rules, clock, initial slots and generation state in one transaction; failure leaves the complete prior state or complete new state.

Use SQLite transactions and its backup mechanism for consistent live DB snapshots. Do not replace an active WAL database using the JSON temp-file scheme. Standalone layout/export files retain temp/fsync/rename semantics. Schema migrations remain versioned, backed up and rollback-safe; never rewrite stored layouts on read.

Use bounded bridge calls (50/page), cancellation and per-source retries. Commit refreshed EPG as a source generation; a partial fetch preserves the last usable cache and user overlays. Profile teardown invalidates late callbacks. Define validated retention, input-size, horizon, channel-count and worker limits before implementation. XML parsing disables external entities; reject unsafe URLs/paths and bound decompression/time ranges. Source data is never executable layout or shell commands. External requests require source enablement and applicable OFF gates; cached/local guide reads initiate none. Distinguish local-LAN Kodi traffic from external provider traffic in diagnostics/tests.

## Delivery and acceptance

T11 retains its dependency and compact-widget → overlay → full-guide progression. Local scheduling is a first-class slice, not postponed behind commercial providers:

1. At implementation entry, read the VM oracle and inspect actual backend/session capabilities through `tools/vm-test.ps1`; preserve existing configurations.
2. Create one native channel → atomic rules/slots → compact view → elapsed-offset Kodi playback → exact return → failed-save recovery. Add real configured Kodi PVR discovery.
3. Complete full EPG/lineup management and scheduling depth: nested/mixed-media rules, fixed blocks, rotation, cooldowns/filler, repairs and clock correction.
4. Add source setup/mapping and external destinations using existing launch/session infrastructure. Promote Plex/YouTube TV capabilities only with actual interface/account/platform evidence. A blocked adapter cannot be called direct integration.
5. Finish shared themed widgets, import/export, accessibility, performance and integrated/offline acceptance. Preserve all attachment and master-plan features.

Retain every attachment acceptance case and master-plan section-10 test: 12-hour-off seek, unchanged seven-day shuffle, Cartoon Network in one save, targeted repair, mixed nested rules, disconnected local use, M3U/XMLTV mapping, remote numbering, DST/clock and interrupted-write rollback. Add no-EPG tuning versus EPG/no-tune, future selection, profile-switch late callbacks, failed/cancelled imports, differing-duration repair, malformed XML, partial refresh and minimum-button external return tests.

Implementation verification requires component/Kodi/offscreen suites and full CTest through the authorized runner, both generic visual fixtures, profile isolation, atomic rollback and disconnected Generator Test. Benchmark cached first paint against the proposed ≤1-second target and 100+ channels on identified hardware. No benchmark or playback pass is claimed here. Planning adds no task completion credit; recompute backlog arithmetic only from actual scope evidence.
