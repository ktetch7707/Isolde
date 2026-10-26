# T11 implementation ledger — 2026-10-26

User authorization: fully implement T11, including the T24 work needed to complete it. This explicitly authorizes the Live TV implementation track while the wider Studio backlog stays open. It does not mark T24's unrelated gates accepted or waive its guide-related editor, fixture, usability and rollback requirements.

## Required completion evidence

The master-plan Live TV Engine sections 1–12 and [reviewed amendment](live-tv-proposal-review-2026-10-26.md) remain the full requirement set. No backend-only slice closes T11.1–T11.3.

| Area | Implementation/evidence required | Current state |
| --- | --- | --- |
| Actual backend audit | Installed Kodi/PVR/add-on version, groups/channels/EPG, identity and tuning, preservation | Source contracts inspected; actual backend audit pending VM availability |
| Shared native store | SQLite WAL, rules/slots/clock/generation state, bounded queries, atomic failure and profile isolation | Initial GuideStore/GuideRules contracts passed; full feature/installed acceptance pending |
| Local creation | Remote rule editor, match preview, mixed media, one-save Cartoon Network, error/retry | Pending |
| Scheduling depth | Stable shuffle/episode order/rotation, fixed blocks, cooldowns/filler, drift/DST, targeted repair, retention and regeneration preview | Initial sequential/shuffle/rotation/real-time/pause and unavailable-slate foundation only; full scheduling depth pending |
| Playback | Actual elapsed seek, current-slot transition, unavailable retry, exact guide return, channel order surfing | Pending |
| Guide surfaces | Compact 3–5 channels/progress → overlay → full timeline, now/next/details/search, truthful capabilities | Pending |
| Source setup | Existing Kodi/PVR, M3U/XMLTV/stream mapping, tuners, Plex/YouTube TV capability verification/app destinations | Pending; provider APIs are not assumed |
| Management/portability | Remote hide/order/number/name/logo/favorites/groups; revisioned drafts/cancel; export/import/remap | Initial backend hide/favorite/order/number/name only; UI/groups/artwork/export pending |
| T24 prerequisites | Shared appearance/editor controls, focus/remote parity, rollback/profile tests, both reference pairs, accessibility/performance/usability evidence | Pending |
| Final acceptance | Full CTest, installed captures and playback, disconnected Generator, source hashes and service restoration | Pending |

## First edit batch

- Added ComponentSchema entry points for versioned local channels and bounded nested rules. No executable content or arbitrary Kodi method execution.
- Added a private profile-local SQLite WAL store with database identity/version checks, full synchronous transactions, committed schedule slots and overlap triggers. Existing unknown databases are preserved.
- Added deterministic seeded per-cycle ordering, episode order and show rotation, persistent cursors, real-time tune-offset resolution and explicit pause/resume checkpoints. This is schedule resolution, not actual playback proof.
- Added revision-checked edits, atomic channel creation/extension/removal, hidden/favorite/order persistence, and missing-media invalidation that retains slot identities/times while excluding removed candidates from later extension.
- Added `guide_contracts` and focused VM suite registration. Tests cover nested/mixed rules, seven-day restart stability, 12-hour offset calculation, interval boundaries, failed saves/extensions, stale revisions, missing-media targeting, preferences, profile teardown, virtual clocks and unknown-version preservation.

These are synchronous store primitives. Production scheduling must run through a bounded worker before large-channel creation is connected to the UI. Fixed blocks and the remaining clock discontinuity/recovery policies are not implemented by this batch.

## Verification log

- `20261026-115339-0c3667`: Check failed at the host network sandbox (SSH permission denied). No guest build started.
- `20261026-115351-683fde`: approved runner access reached the guest; Check reported BUSY. Existing `kodi-movie-art-20261026-d` was verified alive and left undisturbed.
- `guide-foundation-20261026-a`: immutable source snapshot was packaged, but Test reported BUSY before build. Status identified concurrent `kodi-movie-art-20261026-e`; no guide test pass claimed.
- `guide-foundation-20261026-b`: queued through the runner's bounded wait without interrupting the active owner. Incremental Linux build passed; `guide_contracts` passed in 0.20 seconds: 10 behavioral cases plus init/cleanup, 12 Qt results, zero failed/skipped. Host evidence: `build/vm-oracle/guide-foundation-20261026-b/report/{ctest.log,LastTest.log,status.json}`. Immutable guest source/evidence: `/home/isolde-dev/isolde-oracle/runs/guide-foundation-20261026-b/`; source manifest digest `eecabda9a4ae96a2d10f632027e43b1cd720147c8c2ea5384b23713701c1e5b9`. Service stayed active with zero restarts; release links unchanged. This focused run does not stage/deploy a product or prove full CTest, real playback, UI or performance acceptance.

T11 remains 0/3 = 0%. This ledger is not a completion or deployed-release claim.
