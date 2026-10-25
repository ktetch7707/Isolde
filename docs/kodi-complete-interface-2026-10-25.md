# Complete Kodi interface through Isolde

User requirement, 2026-10-25: every ordinary media workflow completes inside Isolde. Kodi is the backend; opening its menus, settings, playlist editor, poster picker or library screens must never be a required step. This explicitly supersedes the October 19 restriction that curation must be performed in Kodi. It extends the current twelve acceptance repairs rather than canceling them or the unfinished VPN work.

## Ownership and implementation

Kodi remains authoritative for media metadata, library organization, playlists, artwork, playback, watched state and resume positions. Isolde supplies the complete remote-operable interface using the existing profile-scoped Kodi service/client/private bridge and validated dispatcher. Do not introduce another media database, scraper or player. Use supported Kodi APIs and the managed profile's supported configuration/playlist interfaces; do not write Kodi's database. No writes to another profile or an unmanaged Kodi installation.

Required workflows:

- Create, name, edit, reorder and delete playlists; choose library members or validated smart rules; preview contents, save atomically and recover on failure. Playlists must be selectable by shelves and retained by Kodi. Existing Isolde-only manual collections are legacy compatibility, not the new playlist backend.
- Browse movies, series, seasons, episodes, music and configured channels; search, filter and browse genres; view details and progress. Actual library data, loading, empty, missing-source and retry states must remain clear.
- View and control playback within the Isolde experience: play/resume/restart, pause, seek, stop, next/previous, audio/subtitle tracks and appropriate viewing settings. Preserve Kodi progress and exact originating shelf/card. Subscription-provider playback remains separately scoped.
- Manage library folders/sources, content type and relevant scanner/metadata settings; trigger refresh/scans, show progress/results/errors, and recover without a Kodi dialog. Make destructive operations explicit and preserve import/backup/rollback guarantees.
- Change a title's poster/artwork from Isolde using Kodi-owned metadata/artwork handling, including preview, cancel, apply, unavailable files and refresh. Do not confuse changing a page component's decoration with changing the library title's poster.
- Expose the remaining relevant Kodi capabilities/settings through comprehensible themed Isolde controls. Keep this capability inventory explicit; a generic raw setting form or an Open Kodi button does not constitute completion.
- Start the backend automatically, keep Kodi navigation/settings invisible, recover backend failures inside Isolde, and provide a consistent remote exit from playback. A separate Kodi menu/window requiring user interaction fails acceptance.

All workflows use the shared appearance model, readable TV sizing and D-pad/Confirm/Back/Menu/Search. Controls have visible focus and exact return; cancel does not write; changes are profile isolated and validated. Report errors with retry and preserve previous working data on failure. Bounded requests and offline behavior remain standing requirements.

## Sequence and acceptance

Finish verification of the current saved-playlist source slice, then extend it into real Kodi-owned playlist authoring. Continue library/source management, scanning and title artwork editing; complete playback controls and automatic background lifecycle. Integrate these with the reported genre shelves and Settings/theme repairs. Existing editor, app-return, Steam, artwork, header/component and screensaver-page requirements remain open.

Maintain native/unit coverage plus real-Kodi tests in fresh disposable profiles. Tests must exercise the actual Isolde UI and verify resulting Kodi data, playback/progress and rollback. An API response, registration, mocked picker, hidden Kodi menu, or a fixture operated through Kodi's UI does not prove the complete user workflow. Both standing visual fixtures and normal-profile preservation remain required. Existing fixtures that use Kodi dialogs may bootstrap synthetic test data, but cannot be counted as evidence that Isolde's setup/management UI is complete.

No implementation-complete claim or new checkbox credit is made by this requirements record.
