# T12 — Kodi-compatible interface sound themes

User direction: extend the consistent TV interaction design with audible remote
feedback, preferably adopting Kodi's sound handling. “Haptic sounds” is treated
as sound feedback, not controller vibration. This is the user-selected sound
slice; the broader cross-screen UI consistency work is not claimed complete.

## Implemented behavior

Settings → Picture & Sound → Interface sounds provides an on/off switch, sound
theme, independent 0–100% effect level, playback policy, preview and refresh.
Default is On at 20%, quiet during playback, using the installed standard Kodi
sound resource when compatible and Soft otherwise. Soft's short, faded tones
are generated locally; no audio binaries or downloaded assets are committed.

Theme arrows choose a local draft; Confirm applies it. Preview uses that draft
without saving it. Back or leaving the page stops preview and discards an
unapplied choice. Applied preferences use a small bounded versioned JSON file
beside the active profile's layout, saved through QSaveFile without direct-write
fallback. Failed saves retain the prior applied settings and expose an error.
Reads, default fallback and refresh never rewrite saved preferences or layouts.

Kodi resource add-ons are discovered only in the active Isolde profile's private
Kodi add-ons and system Kodi add-on directories. There is no scan of another
profile or the person's unrelated `~/.kodi`, no download/install, no add-on
execution, no JSON-RPC sound dependency and no Kodi configuration/DB/skin writes.
The same sound choice is used throughout the Isolde shell. It does not follow
Kodi's currently selected theme automatically; installed packs are selectable
directly in Isolde. External apps, including the native Kodi interface, own
their own sound feedback.

Mappings follow Kodi's action-based model: up/down/left/right, select, Back,
menu, search, Home and errors where mapped. Kodi notification activation is
also readable. Missing mappings in a selected pack stay silent; they are not
replaced with arbitrary Kodi window IDs or sounds. A select cue acknowledges
input, not successful completion. ActionDispatcher/session rejection triggers
the distinct error cue. This does not claim feedback for every asynchronous
subsystem failure or pointer-only action.

Qt Multimedia's existing QSoundEffect dependency preloads and reuses clips;
there is no per-key subprocess. Only one cue plays at a time, repeat bursts are
bounded to a 55 ms minimum interval, duplicate errors are suppressed for 100 ms,
errors supersede a preceding navigation cue, and audio that becomes ready more
than 150 ms after the request is discarded. Preview waits for each clip's
duration before advancing. Master mute, zero effect level, app/window focus,
ambient mode, capture, desktop/native app ownership and the playback preference
gate playback. Policy changes stop pending/current cues. Output-device changes
stop previews and retarget the reusable effects to the system default output.
Profile teardown stops feedback and removes its private temporary audio cache.

## Supported data boundary

- Qt 6.4+ / existing Qt Multimedia, with runtime behavior still awaiting VM proof.
- Unpacked `resource.uisounds.*` add-ons with matching folder/manifest identity
  and the `kodi.resource.uisounds` extension. Both documented metadata extension
  spellings are accepted. Other executable extension types are rejected.
- `resources/sounds.xml` and local relative WAV paths. No ZIP import, URLs,
  traversal, symbolic links, DTDs or custom entities. Mapped assets are validated
  and copied into a private SHA-256-addressed temporary cache before playback.
- PCM WAV, 8/16/24/32-bit, mono/stereo, 8–96 kHz, at most two seconds and 512 KiB
  per clip. Malformed RIFF lengths, unsupported encodings and missing clips
  fail before adopting the theme.
- XML is capped at 64 KiB per file, mappings at 128, installed pack inspection at
  64 across at most four supplied roots, and events at the fixed 11 declared
  semantic events. Discovery reads bounded metadata; audio validation is lazy.
  Cache pruning retains only current/next prepared assets from its own tracked
  temporary paths, bounding it to two sets of at most eleven 512 KiB clips.
- Unavailable or incompatible saved packs use Soft with truthful status while
  preserving the saved choice. Refresh can recover a repaired pack. No Kodi
  sound assets are redistributed by this change.

Primary compatibility references: [Kodi UI sound resource format](https://kodi.wiki/view/UI_sounds_add-ons),
[sounds.xml action mappings](https://kodi.wiki/view/Sounds.xml),
[add-on metadata](https://kodi.wiki/view/Addon.xml),
[Qt low-latency sound effects](https://doc.qt.io/qt-6/qsoundeffect.html) and
[Qt output-device change notifications](https://doc.qt.io/qt-6/qmediadevices.html).

## Verification status

The user's test/build/VM deferral remains in force. No native compilation,
unit/CTest execution, installed journey, visual review, sound-capture or audible
playback result is claimed. Source implementation is complete for this slice;
runtime correctness and integration remain unverified. No deployment or new
T12/project acceptance credit is awarded.

Prepared `interface_sounds` native regressions cover options/WAV bounds, Kodi
mapping and path/DTD/symlink rejection, atomic rollback, no-write-on-read,
profile isolation, private asset copying, repeat/error priority, master mute,
playback/external suppression, preview cancellation, broken-pack fallback and
recovery, and teardown cleanup. The navigation regression covers theme draft,
preview and explicit apply. The fake audio sink deliberately makes no audibility
claim. Static checks cover JSON parsing, embedded JavaScript syntax, whitespace
and progress-table reconciliation only.

When testing resumes, run the focused native/navigation checks, fix the first
named failure, then one coherent full integration. Review the actual Settings
page in dark/pastel/enlarged-text configurations, exercise installed Kodi pack
selection/preview/reopen and two-profile/Back/Home flows, prove the real Qt audio
path using a VM virtual output/loopback recording, and confirm silence under mute,
external-app focus, playback policy and profile teardown. Physical speakers or
controller hardware are not a gate; actual VM audio consumption is distinct
from a mock cue-request pass. Reuse unaffected standing evidence where valid.

Source review additionally corrected a stale launch-guard message that still
directed users to System → Media Library after the Settings reorganization.
