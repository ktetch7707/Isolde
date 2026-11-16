# Managed Kodi source interface

T04 source/content editing uses Kodi's own `CMediaSourceSettings` and
`CVideoDatabase` interfaces inside the existing Kodi process. Isolde does not
write Kodi SQLite tables, run an alternate scraper/player or drive a hidden
Kodi settings dialog. The small native extension is GPL-2.0-or-later source in
`native/kodi`; the existing private Python bridge validates the user operation.

The supported backend is Kodi 21 Omega, built from the installed distribution's
authenticated APT source, including its distribution patches. The API reference
baseline is 21.2; the current VM distribution is 21.3+dfsg-1ubuntu1. The build
receipt records the exact installed version, source descriptor hash, extension
hash, executable hash and generated Debian package hash. The root-owned package
installs under `/opt/isolde-managed-kodi`. Managed mode verifies its manifest
and executable hash before selecting it; ordinary Kodi application mode keeps
the existing distribution launcher.

The VM runner's `Configure -ConfigureTarget kodi-managed` builds and installs
this dependency. That setup is not application acceptance. Preserve the
generated package, authenticated source descriptor/archives and extension
source for redistribution; consumer-image/package delivery remains the separate
installer scope. A stock Kodi installation still supports existing browsing and
playback, but source editing reports that the managed interface is unavailable.
It must not silently claim that folder access alone configured a library.

## Declared source matrix

Creation and editing support this profile's explicitly granted, canonical local
single-folder sources. Video content is Movies, TV shows, Music videos or
unspecified; providers come from Kodi's enabled installed scraper inventory.
Controls expose recursive scanning, folder-name matching, update exclusion and
scan exclusion. Music sources use Kodi's recursive embedded-tag scanner and a
separate music scan action. Existing network/multipath definitions are preserved
and shown read-only. No media files are moved, renamed or deleted; removing a
source preserves existing titles, watched state and resume positions.

Every change has an unsaved draft, read-only preview and explicit confirmation.
The bridge checks the preview's source revision and current ownership again at
apply. Kodi checks both scanner queues before a mutation. Source XML is saved
atomically; a private fsynced journal retains the exact previous XML and provider
settings/scanner metadata until completion. Failed writes and interrupted
transactions restore the old state before another edit. Folder permission
editing waits for a clean idle backend exit, resumes on leaving the editor and
never changes frontend/startup preferences or forces a shutdown.
Desktop import/rollback uses the same asynchronous ownership lease. It waits for
clean shutdown before copying, cancels without writes while waiting, and resumes
the backend after releasing its snapshot lock.

## Playback and capability matrix

Native video and music Menu keys notify the existing managed bridge. Isolde
opens its own themed controls window; no Kodi OSD menu is required. Common
controls are pause/resume, supported seek, stop and return. Movie, episode and
song lists support previous/next, including page boundaries, within the current
filter/playlist. Scheduled channels and legacy manual/history collections keep
their existing navigation. Stop saves Kodi progress before an adjacent title
starts; Home/Back can cancel the transition.

Video controls expose Kodi's reported audio/subtitle tracks, subtitle Off and
Normal, Zoom, Stretch 4:3, Wide zoom, Stretch 16:9 and Original view modes.
Each read/change rechecks exact active-player ownership. Music exposes common
transport controls without unsupported video choices. Closing controls restores
the native playback window; stopping restores the originating Isolde view.
Settings' capability guide is validated declarative data at
`config/kodi-capabilities.json`, with direct setup/recovery actions and explicit
locations for browsing, playlists, posters, system audio and other settings.

Implementation and prepared test assertions are not evidence of a pass. Current
source/VM acceptance and failures remain in the execution queue and campaign
ledger; percentages advance only after their declared evidence gates pass.
