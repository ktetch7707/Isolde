# T04 Kodi verification — November 12

Current installed candidate: `t04-integration-20261112-f`, release
`58a7c894d6ffc1b1e158d68e`, source digest
`d0b2e32ed2689dd2728ce188cece041a974dd7e644ff7e8f35673f75ec10bf9b`.
All63 registered CTests pass. Original e also passed63; its installed playlist
journey exposed the episode-dispatch defect, retained in the campaign report.

Actual software fixes include recursive immutable desktop/prior-profile backups,
private-profile import and rollback, crash recovery, cancellation and backend
ownership/locking; playlist Open, retained management selection, asynchronous
refresh and complete-permutation rejection; episode playlist playback dispatch;
and import Back handling. Native import evidence is14 filesystem/SQLite/crash
contracts plus35 media-client cases. Import UI acceptance remains open.

Installed playlist attempt `203323-181`: all seven frames reviewed. The original
remote name/cancel/save workflow creates a real Kodi smart playlist. The catalog
opens actual episode members. Confirm starts fullscreen color-bar Episode1x1,
Confirm pauses, and managed Back stops after the Kodi OSD animation settles.
Return visibly selects the same Episode1x1 in the same playlist. Media hashes
and both normal layout hashes are unchanged; service active/running, zero restarts.
This earns T04.4.c only; complete authoring/management/shelf acceptance stays open.

Installed movieart attempt `204701-920`: reviewed Movies list and real Big Buck
Bunny poster/metadata. Kodi's TMDB provider owns matching and artwork; the bridge
returns and decodes its actual image. Original media is unchanged. This proves
read/display, not title-art editing, source/content management or Kodi-free setup.
Earlier invalid playback capture and readiness failure remain recorded.

The extra Studio desktop case proves top-level remote/pointer byte equality and
cancel, but its nested input-group authoring path is stale (BUG-138). No complete
nested-pointer acceptance or Kodi DesktopSession claim is made. Six pre-existing
Studio test modifications remain unrelated and are excluded from these commits.

Acceptance: T04=45%; project2198/25=87.92%, rounded88%;191/228 milestones.
Next: finish T04 inventory and installed preservation flow, playlist management/
authoring, source/content/title-art controls, capability inventory and fresh setup.
Do not rerun the full application suite for documentation or helper-only changes.

Current runner maintenance self-tests:123/123 pass, including movieart helper identity/flags and frozen dependencies. Verified guest cleanup reclaimed1,552,723,968 bytes, leaving2,147,745,792 bytes (2 GiB test minimum met;3 GiB cleanup target not met). Configure reports its target failure honestly; application/test acceptance is unaffected. No host cleanup. No runner remains active.
