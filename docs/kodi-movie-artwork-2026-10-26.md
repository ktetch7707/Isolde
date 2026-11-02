# Real movie recognition and artwork fixture

User-requested T21 artwork prerequisite, 2026-10-26. A generated test-pattern video is named `Big Buck Bunny (2008).ogv` inside a matching movie folder. It contains no frames from the actual film. Kodi's TMDB Python provider must identify it from the folder/title/year, populate its own library, and cache genuine provider artwork. No movie NFO, imported movie metadata, supplied poster, direct database write or Isolde scraper is used.

The opt-in bounded runner case is `movieart`:

```powershell
./tools/vm-test.ps1 Test -RunId <new-id>
./tools/vm-test.ps1 Visual -RunId <same-passing-id> -Cases movieart
```

It creates a fresh UUID native Kodi profile, synthetic media beneath `~/Videos/Isolde-Acceptance/`, and temporary Isolde configuration. Kodi's native library import loads **source configuration only**, followed by Kodi's own scan. The existing Household and Studio Demo libraries are not used. The test requires title/year, TMDB ID `10378`, provider artwork URLs, valid decoded image bytes from `Isolde.GetArtwork`, unchanged synthetic media, and a captured Isolde Movies view. Playback and physical qualification are explicitly unverified. The service and normal layouts must be restored by the guardian; no deployment occurs.

Kodi documents [Title (Year) movie naming](https://kodi.wiki/view/Naming_video_files/Movies), [source scanning and artwork downloads](https://kodi.wiki/view/Adding_video_sources), and the [TMDB Python provider](https://kodi.wiki/view/Add-on:The_Movie_Database_Python). Source setup uses Kodi's export/import path format, implemented by its own `VideoDatabase::ImportFromXML`; movie identity and art are not imported.

## Diagnostics retained

- `kodi-movie-art-20261026-a`: full suite **34/34** passed; actual scan identified Big Buck Bunny (2008), IMDb `tt1254207`, TMDB `10378`. Artwork gate failed: only a generated video thumbnail was returned. Logs show Kodi updating its bundled TMDB provider to 3.2.2 during the scan and reporting missing artwork settings. The working diagnosis is that importing source settings before the provider update preserved an incomplete configuration. Normal layouts stayed byte-identical; service restored active/running with zero restarts. Logs: `build/vm-oracle/kodi-movie-art-20261026-a/`.
- `kodi-movie-art-20261026-b`: host packaging refused another coder's untracked Games files before guest testing. No test pass claimed. A detached checkout under `build/kodi-artwork-worktree/` isolates this harness from that unfinished work.
- `kodi-movie-art-20261026-c`: isolated checkout compiled, then `audio_backend` failed and timed out because Windows checkout converted its Python mixer fixture's shebang to CRLF. The main checkout's fixture was LF. Byte inspection confirmed the difference. The isolated checkout's text files were restored to their repository LF endings, retaining only the four artwork harness changes. `.gitattributes` now requires LF for Python/shell scripts; Git checkout-filter checks with `core.autocrlf=true` pass for both audio fixtures and the Kodi harness. No audio application behavior was changed.
- The corrected fixture waits for the provider's poster settings to exist and explicitly enables its artwork settings through Kodi's addon API **before** importing the source configuration.
- `kodi-movie-art-20261026-d`: full suite **34/34** passed, including audio. The corrected scan matched the movie, returned TMDB artwork URLs and delivered a valid decoded image through Isolde's bridge. The journey then failed its unchanged-file assertion: its baseline incorrectly preceded the deliberate source XML replacement. The reviewed capture also exposed incomplete fixture navigation: Movies was focused while Shows remained open. Both harness issues were corrected before the next run; no end-to-end artwork pass is credited to D.

`kodi-movie-art-20261026-e` passes **34/34** in 247.75 seconds after those harness corrections. Frozen source digest: `6fc8d0a174ffaf0274d4af968c498b862ccf89084d0013fa73c7c72161611210`; archive SHA-256: `6c5817860af5f5f1346a2761c8206ef45900537eb4d5651349043a4f51256eb6`. Local evidence root is `build/kodi-artwork-worktree/build/vm-oracle/kodi-movie-art-20261026-e/`. Initial graphical attempts were refused BUSY while the Guide and emulator guardians owned the VM; neither was interrupted. All 29 native PNGs in E (and the earlier full A/D runs) are SHA-256 identical to the 29 reviewed native captures from `studio-overflow-icon-20261026-full-g`; no new native visual differences were found.

Final artwork acceptance is pending. T21 remains 36%; Overall remains 32%. This fixture alone does not close broad component, provider, visual or final software acceptance gates.
