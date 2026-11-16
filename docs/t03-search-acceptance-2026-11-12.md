# T03 installed local-search acceptance — 2026-11-12

T03 accepted100%; project2189/25=87.56% rounded88%,188/222 milestones.
Installed release `f21c113eb646a6997202177b`, candidate
`t03-search-20261112-x`, source digest
`a8cadc9429767ee1999c2d13f2bd4881733cb804905c6dd5770aa6164bdb0cc1`.

## Evidence and review

Original full p62/62 passes. Final x5/5 focused registrations pass, with
checksum-verified original full reuse. Main changes beyond p are restricted to
three exact origin-visibility edits: a transient boolean, capture before playing
and restoration before reopening Search. Unique original anchors and normalized
full-text equality reject any other Main edit. Both frozen files' byte hashes
must match their manifests.123 runner tests pass, including guard positive and
negative cases; other C++/config/routing changes remain refused.

Actual installed journey:
`build/vm-oracle/t03-search-20261112-x/report/evidence-local-search-1794530432664843991/isolde-tv-acceptance-zbditjg_/result.json`.
Nine captures reviewed at `visual/local-search/attempt-20261112-163901-571`;
contact sheets `build/vm-oracle/round2-20261111-b/review/local-search-final-x-00.jpg`
and `local-search-final-x-01.jpg`.

A fresh private Kodi profile imports synthetic local media through native Kodi
XML import, with no database writes or copied personal profile. Real Kodi owns
one show and56 episodes. Kernel AF_INET+AF_INET6 denial returns ENETUNREACH in the
actual process before exec; AF_UNIX remains available. All input is keyboard
remote navigation and the production onscreen keyboard. Search Web stays Off.

| Capture | Agent observation |
| --- | --- |
| origin | Library Shows screen, Movies toolbar control selected |
| entry | Search opens above that screen; Web Off |
| results |52 rows: show,50 episodes and explicit More action |
| more | More Episodes selected and completely visible |
| next |8 rows including Previous and results51–56 |
| previous | Previous returns first page and selected episode row |
| playing | Real Kodi decodes the local Theora color-bar video, Episode1x50 |
| playreturn | Same query fixture, page two, Episode1x50 selection restored |
| returned | Back restores the originating library screen and toolbar focus; Kodi's own refresh now exposes imported show |

Normal layout hashes unchanged; normal service active/running/zero restarts.
No live-provider, DRM or physical-device claim. Original native page-source,
debounce/cancel, preference/profile and write-failure contracts remain covered by
p's full run; final actual QML regressions preserve query/selection, use a real
window with selected row y>4000 and require complete viewport containment.

## Defects and honest failures

BUG-127: stopping Search playback lost Search and underlying-library origin.
Fixed by explicit return notification, preserving local result clients/page and
focus state, and restoring the originating library visibility. Physical Home
continues to go Home. BUG-128: index*82 scroll estimate disagreed with actual
76+10 row geometry; the selected More row could disappear. Fixed with actual
delegate geometry. Both fixes are visible in the installed final captures.

Fixture repairs: await exact56 imported episodes, actual artwork completion
instead of200ms sleep, and both independent history-client row counts after
session notifications. Failed q/r/s/u/w and earlier installed attempts remain
recorded in the campaign; they are not passes.

BUG-125 remains an intermittent offscreen Studio frame-stall diagnostic: full h
and v fail the250ms maximum with ~500ms1080p samples; unchanged isolated i and
full j/p/u performance pass. No timing limit changed, no claimed Studio fix and
no claim that v passed. Final x reuses identical Studio/performance source bytes
from original full p; final S1 risk review must retain this unresolved diagnostic.
