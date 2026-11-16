# App icon implementation — November 10, 2026

User scope: implement the accepted icon design, preserving selection glow that
follows the icon shape. Scope is app artwork, its shared renderers and Studio
controls; no Kodi media artwork, layout migration or installed release change.

## Implemented

- Twenty explicit foreground SVG resources and a versioned role manifest.
  Original branded artwork remains untouched. Source/foreground hashes,
  optical scale, contrast backing and negative-space colours are declarative.
  The authoring tool is `tools/prepare-app-icon-roles.ps1`; runtime processing
  no longer recognizes SVG wrappers with a regex.
- Full-colour foregrounds and separate cached ink masks. Declared internal
  knockout colours preserve details when tinted. Apps without a reviewed mask
  retain Brand artwork and are labelled accordingly instead of silently
  flattening their artwork.
- Signature, Quiet tiles and Compact rail are validated preset data. New app
  rows use Signature. Existing layout bytes are not rewritten. Existing
  cutout/original/tile/monochrome treatments remain available.
- Shape-following contour and soft glow derive from foreground alpha. The
  padded drawing area keeps the effect inside the renderer without changing
  layout geometry. Deselection clears both effects. No-art shortcuts retain
  a visible frame; Frame is also an explicit alternative. Width, spread,
  intensity, colour, padding, size and backing reuse Studio presentation data.
- Light surfaces can use a declared contrast backing for light-coloured marks.
  Colour foregrounds are not silently recoloured to improve contrast.
- All Apps also uses foreground artwork and selected shape glow. Custom
  full-tile artwork keeps its existing rendering role.
- Imported still logos offer Original/Clean background, bounded strength and
  explicit backdrop choices with light/dark/pastel previews. Cleanup works on
  a bounded up-to-512px copy, classifies quantized edge colours, follows
  connected backdrop regions and reconstructs matte edges from original RGB
  in linear light. Existing transparency, internal white details and uncertain
  images are conservatively retained. This is not general photographic
  segmentation or a claim of perfect extraction for every image.
- Preview/cancel changes no applied document. Cleaned images use the existing
  private staging budget, content hashes and atomic Apply/rollback path. Apply
  retains the corresponding original in the profile image library as well as
  the derivative; external original files are never overwritten.

## Verification record

Focused suite `app_icons` selects native app/component contracts, Studio
artwork/selection and Home/Studio navigation. It does not select the deferred
emulator campaign or claim a full-suite pass. The user explicitly authorized
focused checks for this implementation.

- Attempt a stopped at the source-snapshot guard because new icon resources
  were untracked. They were tracked before retrying; no build was attempted.
- Attempt b stopped at the runner's theme-fixture equality check. The stored
  fixture was updated from the new app-row preset, preserving its test-specific
  visible-column count. No application test was attempted in b.
- Attempt c compiled. The first registration found that no-art authored orb
  shortcuts had lost their frame fallback. Fixed that actual regression.
  The attempt remains a recorded failure; later registrations were not run.
- Attempt d stopped before snapshot/build because another active agent was
  adding interface-sound source files. Those files were included with Git
  intent-to-add only, without editing or committing their work.
- Attempts e/f were blocked by concurrent interface-sound work before tests.
  Verification moved to the isolated native worktree `build/icon-check` at
  base `171f70b`, containing only this icon batch.
- Attempt g caught Qt Canvas tinting not copying pixels. The explicit dirty
  rectangle fixes tint while retaining alpha and focus colour.
- Attempt h passed selection/artwork, then caught a noisy white matte fringe.
  Least-squares RGB projection now recovers edge alpha without using a weak
  single colour channel.
- Attempt i passed selection/artwork/core and original recovery. Component
  contracts stopped on a pre-existing settings-order fixture missing its
  composition schemaVersion. Valid and invalid-order fixtures now supply
  version1. Production validation is unchanged.
- Attempt j is the final focused candidate; its result is recorded below.

Prepared regressions cover explicit foreground resolution/transparency,
negative-space masks, noisy white backgrounds, matte reconstruction, conservative
fallback, parameter bounds, remote cleanup/cancel, original recovery, profile
teardown, exact deselection, no-art focus, all three styles, tint and glow pixels.
Native icon and QML captures are retained by the VM runner for agent review.

No project acceptance percentage is awarded from source changes or focused
checks. No full suite, deployment or installed acceptance is claimed here.

## Final focused result

`build/icon-check/build/vm-oracle/app-icons-20261110-j/report/ctest.log`:
**6/6 registrations pass, zero failures**, 38.94 seconds. Runner self-tests102
pass. Source digest `8ce949871cf291b068e5ea09b134cfeca589ea2da7df78acc886f12dafca4f50`.
The isolated candidate excludes the separately committed sound implementation;
no combined sound/icon or installed acceptance is inferred.

Agent reviewed actual Qt captures `studio-test-icon-styles-pastel.png` and
`studio-test-icon-styles-dark.png`: three presentations, production512px cached
foregrounds, light-mark backing and selected silhouette glow. Rendered-pixel
checks verify theme tint, internal holes, glow beyond the contour and exact
clearing on deselection. Native tests verify all20 foreground resources and
19 cached foreground/mask pairs. Individual derived-pair screenshots were not
collected because their camel-case filenames fall outside the report allowlist;
their native assertions passed, but no individual visual review is claimed.

Progress accounting `tools/backlog-progress.ps1 -CheckTable` passes:
1563/25=63%, unchanged. No deployment performed.
