# Kodi library for designing real Studio carousels

## November 10: real Kodi artwork

The user requested replacing placeholder artwork in the existing persistent demo.
The fixture now identifies its eight synthetic movie clips as The Matrix,
Inception, Interstellar, The Fellowship of the Ring, Spirited Away, Gladiator,
Star Wars and Toy Story, and its shows as Once Upon a Time and Glee.
Kodi's installed TMDB scrapers resolve URL NFO identities and fetch their own
posters, fanart and episode images, including scraper-selected fanart.tv assets.
The clips remain synthetic; their plots explicitly say so.

Run through the supported, hash-verified VM runner against the currently deployed
tested candidate:

```powershell
./tools/vm-test.ps1 Configure -ConfigureTarget kodi-demo-art -RunId ui-audit-20261110-f -WaitSeconds 900
```

This target only supports the exact existing Studio Demo UUID on Isolde-Test.
It preserves original metadata/history in `real-art-before.json`, original
placeholder files beside the media, and profile/layout/release identities.
It uses Kodi's native import UI, internal JSON-RPC refresh/setters, its normal
artwork cache, and Isolde's existing artwork bridge. There are no database
writes, separate image downloads, new dependencies or application changes.
Already-scraped entries use a history-recovery path rather than refreshing again.

Evidence is under `build/vm-oracle/ui-audit-20261110-f/report/`:
`kodi-demo-art-result.json`, `demo-art-history-after.json`, and ten
`studio-test-demo-art-*.png` images returned by the production artwork bridge.
Earlier failures and recovery diagnostics remain recorded. Native refreshes
replace IDs asynchronously, so the helper follows exact media paths and requires
the intended title plus primary remote artwork. A prior interrupted refresh
was recovered with Kodi's configured-source scan. Repeated fixture restarts
also reached the guest service's start-rate limit; the helper records that
journal before resetting only that diagnosed guest service failure.

The older seeder's four-resume assumption is historical: the actual pre-change
backup already contains three resumable movies after earlier playback. Final
verification compares each movie and episode's history to that backup instead.
The final Configure run passed: `restartVerified`, `watchHistoryUnchanged` and
`layoutsUnchanged` are true; ten artwork responses contain 34–95 KB each.
All eight movies and all six episodes match their saved watch/resume histories.
The service is active/running with zero automatic restarts, on unchanged release
`742be8322eff80e0047e9d85`. The Matrix poster returned by the bridge was opened
and visually reviewed. This is persistent fixture/artwork verification, not a
claim that the broader Studio or project acceptance gates are complete.
No software acceptance percentage changes from this development-data setup.

Kodi documents this identity mechanism in
[parsing NFO files](https://kodi.wiki/view/NFO_files/Parsing); the native library
operations use its [JSON-RPC API](https://kodi.wiki/view/JSON-RPC_API/v13).

The sections below describe the original fictional seed and are retained as
historical setup evidence.

The VM has an active **Studio Demo** profile, ID `8bdf6077-4613-4b52-9b20-d1ec7931dfcf`. Household and alex remain registered. Household's layout hash is unchanged; its Kodi library was not imported into or edited. No application build was installed. The service is active with zero automatic restarts.

## What is available

Eight movies: Moonlit Harbor, The Glass Garden, Beyond the Clouds, Postcards from June, A Quiet Orbit, The Last Lighthouse, Wildflower Road and Sunday in the City. The first four have resume points; the next two are watched; the last two are unwatched. Each has distinct local pastel artwork.

Two shows, Skyline Stories and The Weekend Diaries, each have three episodes: episode 1 watched, episode 2 partially watched at 45/120 seconds, episode 3 unwatched. These are fictional metadata over small playable 120-second synthetic clips, not copies of commercial titles. All fourteen file entries reference local video; hard links share the underlying clip to keep disk use small.

All library entries, artwork references, play counts, last-played dates and resume points belong to Kodi. The native Kodi import UI added the entries; a fixture script running inside Kodi used its internal JSON-RPC setters for only the new fixture IDs. No direct database writes, application bridge expansion, HTTP listener, scraper or download was used. Media stays read-only inside Kodi; fixture scripts/results use the new profile's writable userdata.

## Use in Studio

In **Studio Demo**, Add component → **Media shelf** → **Movies · Continue watching** gives the four resumable movies. **Movies · Recently added** gives all eight in date-added order, enough to exercise horizontal overflow. **Movies · All** and **Shows · All** use the same real library. Set the shelf's layout to `row` for a carousel, then edit its presentation and placement as desired.

The existing `recent` query means **recently added**, not watch history. Seeding last-played dates does not change that application behavior. Continue Watching filters in-progress entries and sorts by last played. App carousels still use installed applications; Kodi sample titles do not create or emulate Netflix/Disney+/other app histories. This data setup does not replace the existing pastel preset's placed illustration buttons with shelves automatically or edit any saved page.

Settings → Profiles switches back to Household. This demo is persistent and can be edited normally. Do not reseed it after use: that would replace the user's accumulated demo watch state. The importer refuses a completed marker and the setup refuses an existing Studio Demo name.

## Evidence

Guest media/evidence root: `/home/isolde-dev/Videos/Isolde-Studio-Demo/8bdf6077-4613-4b52-9b20-d1ec7931dfcf/`.

Guest Kodi userdata: `/home/isolde-dev/.local/share/Isolde/native-profiles/8bdf6077-4613-4b52-9b20-d1ec7931dfcf/kodi/home/.kodi/userdata/`.

- `manifest.json` records identity, original release, Household layout hash and imported library IDs.
- `verification.json` confirms the actual application bridge returns four Continue Watching movies, eight Recently Added movies, eight nonempty local PNG/JPEG artwork responses, and the correct episode watch/resume state for both shows.
- Host copies: `build/studio-demo/{manifest,verification}.json`; `build/studio-demo-verify.log` and `build/studio-demo/decode.log`. The shared video fully decoded locally through GStreamer without changing Kodi history. One representative artwork image was opened and reviewed at `build/studio-demo/landscape.png`.
- Python compilation passed for both fixture tools inside the VM. Earlier failed runs are preserved in `build/studio-demo-seed*.log`; failures restored the prior profile/grants. The native X11 fatal-error case was explicitly restored using the saved baseline. Three prepared media roots remain; only the completed profile above is registered. No failed artifacts or old build payloads were deleted.

This is seeded-data verification, not new Studio UI acceptance. The pending selection/slider build and fixture gates remain pending. T21 stays 36%; Overall stays 35% across 24 active tasks.

## Repeatable process for a fresh VM demo

Use the existing PowerShell transport in `tools/vm-process.ps1`, as in the VM guide. Copy **both** `seed-studio-demo.py` and `studio-demo-kodi-import.py` together into a guest tooling directory. Run under the guest desktop user:

```sh
python3 seed-studio-demo.py --execute-on-isolde-test-vm
# Only to verify the existing completed profile; no history/profile changes:
python3 seed-studio-demo.py --execute-on-isolde-test-vm --verify-profile 8bdf6077-4613-4b52-9b20-d1ec7931dfcf
# After an intentional deployment, name its exact release ID explicitly:
python3 seed-studio-demo.py --execute-on-isolde-test-vm --verify-profile 8bdf6077-4613-4b52-9b20-d1ec7931dfcf --expected-release a72852697c1f50e138161a63
```

The tool locks against the Oracle and graphical harness, requires the existing guest home and running service, creates a fresh UUID profile, records baseline backups, and briefly restarts the same installed release to activate it. On ordinary failure it restores the original active-profile registry/media grants and restarts the existing service. It preserves failed data for diagnosis. The `--restore-prepared-profile UUID` recovery mode requires that exact prepared profile to be active; `--resume-prepared-profile UUID` requires matching unchanged baseline configuration and an unfinished fixture manifest. Never bypass these checks, delete locks, or copy an existing Kodi database/profile.

The tools reuse already-installed GStreamer/Pillow/X11/wmctrl; they install no dependencies. Initial setup needs no internet. Native GUI import requires an unlocked graphical VM session. It never performs VM snapshot/reset or host configuration operations.
