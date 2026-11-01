# T21.8 — bounded managed GIF implementation

This source batch follows [the Studio controls checkpoint](studio-controls-2026-10-27.md). VM/native/visual testing remains assigned by the user to another agent. No acceptance checkbox or completion percentage changes here.

`ManagedGif` supplies one decoder boundary for Studio import, runtime still/playback and skin import/export. It uses Qt's existing image decoder; repeated `QImageReader::read()` calls advance animation frames as documented in the [Qt API](https://doc.qt.io/qt-6/qimagereader.html#read). No dependency, executable layout data, remote image request, separate media store or persistent animation cache is added.

Bounds are explicit and refusal leaves the original draft/applied layout intact:

| Resource | Limit |
|---|---|
| GIF file | 2 MiB |
| Canvas dimensions | 1024 × 1024 |
| Frames | 1–60 |
| Cumulative decoded frames | 4 MiB |
| Playback PNG frames | 2 MiB before base64 encoding |
| Frame delay | 20–1000 ms |
| One loop | 30 seconds |
| Decode and frame preparation | 250 ms, checked between frames |
| Concurrent animation leases | 8 per profile/draft resolver |
| Validated still cache | 8 MiB per resolver, charged by string storage |

The time limit is cooperative between bounded frame operations, not process-level preemption of an individual Qt decoder call. Pixel, frame and encoded-byte limits bound each such operation. Unsupported codecs or invalid frames are refused with an error. Installation/runtime evidence is still required.

GIFs stage privately under a content-addressed `.gif` identity through the existing atomic asset path. QML receives a validated PNG still from `resolveArtwork`, never a raw GIF filename. Even the default still therefore passes the full GIF validation boundary. Still-cache keys use content hashes, and loading a profile or invalidating its Studio model clears caches and leases.

Image components default to a still frame. Content → Play GIF animation is a validated boolean override. Playback uses bounded prepared PNG frames through the existing crop/focal/fit/rounded Canvas renderer, unloading the previous Canvas image. It observes GIF loop counts, shared reduced motion, actual window activity, clipped ancestor bounds and page/app/editor/modal state. Hidden/offscreen playback releases its lease and frame array; returning restarts from the still/first frame. Images beyond the active lease budget remain still with an error rather than decoding unbounded animation data. Disabling and re-enabling animation, or leaving and re-entering the viewport, retries.

Image links, labels, accessibility and placement are unchanged by motion. Replacement into an incompatible component drops motion through the existing loss-review transaction. Skins retain portable managed bytes and validate GIFs before export/import; page/header backgrounds remain still PNG/JPEG features.

Host verification: `node build/studio-finish-20261027/verify-source.cjs` passes 276 extracted JavaScript functions and 89 handlers, including actual playback-controller methods for default still, finite loops, inactive window, clipped/offscreen pause, reduced motion and lease release. [Source hashes and scope](quality/studio-gif-host-20261027.json) include native files but **do not claim native compilation or decoding**. `git diff --check` passes.

Added native case `studioGifArtworkIsBoundedStagedAndProfileScoped` constructs a real two-frame GIF, checks frame/dimension/byte/delay/loop bounds, staged no-write behavior, safe still conversion, eight-lease refusal/release, cross-model isolation and teardown. It is included in `studio_unit`. Added QML case `test_gif_motion_default_pause_and_release` checks the controller with deterministic prepared frames. Both await actual Qt execution.

Testing handoff: use `docs/vm-oracle.md` and the bounded runner. Run new native and QML cases, then full CTest and both reference/reset pairs. Add an actual linked GIF import → still → opt-in motion → crop/rounding → reduced motion → scroll offscreen → app background/return → cancel/undo/Apply → saved-design export/import journey, with source identity, frame captures and unchanged profile bytes where cancellation is expected. Stop at the first named failure. No installed GIF pass is recorded here.
