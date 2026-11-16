# Isolde app icon system — design proposal

Status: implemented; focused VM checks pass6/6. See [implementation and evidence](../app-icons-implementation-2026-11-10.md). Installed acceptance remains separate.

## Direction

Use crisp, recognizable foreground logos with balanced visual size, generous
spacing, quiet labels and stable focus. Default to original brand colours on
transparent backgrounds. Offer Theme ink as a deliberate alternate treatment.
Keep cleanup independent of colour styling so a tint cannot conceal faulty
background extraction. White can be real artwork or contamination: colour
alone is never sufficient evidence to erase it.

Three compatible presentations use the same artwork pipeline:

| Presentation | Visual and interaction | Intended use |
|---|---|---|
| Signature — recommended default | Transparent foreground logos, labels underneath, generous spacing, selected glow following the logo shape | Favourite app rows and a spacious TV Home |
| Quiet tiles | Small logos in consistent neutral tiles, names beside them, selected logo-shaped glow | Dense All Apps and busy photographic pages |
| Compact rail | Small logo and name inside a restrained capsule, selected logo-shaped glow | Narrow rows and secondary app groups |

The review board compares these directions using six existing bundled provider
assets. It demonstrates treatment, surfaces and selection only. It is not a
noise-removal demonstration, a Qt rendering pass, or proof of approved brand
masks. Preview masking uses foreground alpha; approved production ink masks
must retain internal negative space as specified below.

The browser-only board was checked at736px and320px: all18 displayed images
decode, all three variants fit horizontally, selection/treatment/surface changes
work, and no page-script errors were recorded. Signature, narrow Quiet tiles,
and a pastel Theme ink rail were visually inspected. This verifies the review
board only; production VM tests and deployment remain unchanged.

## Source assets and roles

Store separate, explicit roles in declarative app artwork metadata:

- **Original:** untouched source artwork, including any intentional brand tile.
- **Foreground:** approved full-colour transparent logo with no accidental
  backdrop, matte or halo.
- **Ink mask:** approved alpha geometry for one-colour use. Interior cutouts
  are deliberately authored; white lettering is not blindly made transparent.
- **Optical bounds:** reviewed visual centre and scale for the icon slot,
  plus safe padding. These affect presentation, never layout placement.
- **Provenance:** original hash/source/licence, derivation version and output
  hashes. Record whether an asset is reviewed or automatically derived.

For the bundled set, produce and review foregrounds/masks at authoring time.
Use existing vector geometry. Describe the backing explicitly in metadata or
separate assets instead of depending permanently on a regex that recognizes
one serialized SVG wrapper. Keep all derivations declarative and offline.
Never turn an external SVG into executable layout data.

Do not confuse silhouette with a good monochrome logo. For example, merging a
light detail into its surrounding coloured shape can make it disappear when
both become the same ink. An approved ink mask must preserve that detail as
intentional negative space or use an approved alternate symbol. When no usable
mask exists, retain the full-colour foreground and make Theme ink unavailable
with a readable reason. Never silently show a solid blob.

## Background cleanup and noise

The user's contrast/posterization suggestion is useful for **classification**.
It must not permanently posterize the displayed full-colour asset. Segment on
a simplified working copy, then reconstruct alpha and edge colour using the
original pixels. Treat the following as an implementation proposal requiring
fixture tuning, not an already validated general segmentation algorithm.

1. **Decode and normalize safely.** Apply orientation/colour management, check
   input size/type, and normalize alpha representation. Preserve the source.
   Do not flatten onto white as an intermediate step. Reuse existing artwork
   limits and reject invalid inputs through the normal error path.
2. **Prefer explicit structure.** For reviewed vector assets, use declared
   foreground paths and masks. For clean transparent raster art, preserve its
   alpha and original colours; do not automatically remove white pixels.
3. **Estimate the backdrop.** For opaque or partially transparent imports,
   sample the perimeter and visible silhouette boundary, including inset
   rounded plates. Cluster candidate colours and examine connectivity. Four
   matching corner pixels are insufficient when those corners are transparent.
4. **Create a classification copy.** Use edge-preserving denoising, colour
   clustering/posterization and local contrast to distinguish coherent
   foreground from background compression noise. Parameters are bounded and
   scale with image resolution. Keep these changes out of final RGB artwork.
5. **Segment conservatively.** Remove only regions supported by backdrop colour,
   spatial coherence and boundary connectivity. Use high/low confidence bands
   rather than one hard threshold. Preserve internal counters and disconnected
   intentional marks. Do not erase a small island solely because it is small:
   dots, stars, letter accents and trademark details can be legitimate.
6. **Recover soft alpha.** Maintain a narrow transition band rather than a
   jagged binary cut or broad blur. Refine against original-resolution pixels,
   preserve thin strokes, and avoid destructive erosion of the whole logo.
7. **Remove matte contamination.** Where a known backdrop was blended into an
   edge, estimate foreground colour from that backdrop and recovered alpha in
   linear light. Clamp numerical instability at very low alpha. Clear RGB in
   fully transparent pixels; avoid double-premultiplying existing alpha. White
   edge contamination must not survive just because alpha became transparent.
8. **Validate and fall back.** Require adequate remaining structure and review
   on light, dark, pastel and checkerboard surfaces. Ambiguous or complex art
   stays Original in a consistent tile; it is never reported as a clean cutout.

Arbitrary photographs, gradients and heavily compressed logos are not promised
automatic perfect extraction. A conservative fallback is preferable to quietly
corrupting recognizable artwork. No downloaded model, service or new runtime
dependency is required by this design.

For imported artwork that needs help, expose **Original / Clean background**,
an optional background-colour swatch, and a bounded Cleanup strength control
under Advanced. Show the derived image immediately on three backgrounds.
Confirm stages it as one draft transaction; Back restores the complete prior
image/options. Do not require a desktop brush, drag selection or an image-editing
wizard. Retain the original so users can always undo extraction.

## Visual rules

- Start with a96-logical-pixel icon slot at1080p, an approximately68–76px
  foreground and24px inter-item spacing. These are preset starting values,
  exposed through shared size/spacing/presentation controls, not renderer
  constants. Downscale deliberately for denser layouts.
- Normalize perceived size, not only image dimensions. A wide wordmark, a
  circular emblem and a narrow letter need different reviewed optical bounds.
  Keep alignment and padding consistent without stretching or cropping logos.
- Do not place a coloured square inside a coloured circle inside another card.
  Signature has no backing by default. Quiet tiles have one neutral backing.
  Intentional official brand tiles remain available through Original.
- Theme ink is optional, using the existing named palette/colour wheel. It
  maps an approved mask to a theme or per-component colour. Theme changes
  never overwrite asset pixels or user overrides. Preserve multicolour brand
  art unless the user explicitly chooses ink treatment.
- Test foreground visibility against its actual surface. For Original brand
  colours, prefer a reviewed light/dark variant or a subtle neutral backing
  when needed, without silently recolouring the brand. If the user insists on
  an unreadable custom colour, show an actionable preview warning.
- Keep labels readable at TV distance; begin at20–22px at1080p, with authored
  typography/density scaling. Allow a second line in gallery tiles. Never
  shrink long labels to force them into a fixed chip.
- User refinement: retain **glow following the selected icon's actual shape**
  as the default focus treatment in every presentation. Derive it from clean
  foreground alpha, never the backing/tile bounds. Combine a narrow defined
  contour with a restrained soft outer glow; keep the logo itself sharp.
  Glow colour, contour width, spread and intensity use the shared appearance
  model. Glow belongs to focus and disappears completely on deselection; it
  must never be baked into original/foreground/mask files or used to hide noise.
  Hit targets and layout geometry stay stable as the glow changes. Preserve
  interior negative space, do not clip the glow at slot boundaries, and ensure
  only the currently focused icon glows. Frame focus remains an optional
  accessibility/contrast alternative rather than replacing the requested glow.
  Reduced motion removes transitions, not the selected glow. A legible selected
  label supplies a second cue for faint or exceptionally thin logos.
- Loading/error states retain slot size and label. A missing asset uses an
  existing semantic app symbol with a readable name, not a random package icon.

All these numbers are design targets for review. They do not claim measured
contrast, runtime performance or completed accessibility acceptance.

## Studio and data contract

Use one compact **App icons** group within the existing component editor:
**Style** (Signature / Quiet tiles / Compact rail), **Logo** (Brand / Theme ink /
Original), **Colour** (when ink is selected), and **Size**. Backing and precise
spacing are under Advanced. Defaults require no configuration.

The shipped data already has `logoMode`, `iconColor`, `iconSize`, `artFit`,
surface, spacing and focus controls. Extend/reconcile these rather than add a
parallel theme system. Preserve existing cutout/original/tile values and their
rendering intent on read. Do not migrate or rewrite old layouts at startup.
New presets use explicit role/appearance defaults. Schema validates enums,
bounds, managed assets and metadata; the renderer executes no asset scripts.

Generate expensive cleanup once at import/cache time, not while navigating.
Cache original and derived images by source hash, processing version, role,
parameters and render size, isolated per profile. Store masks separately from
colour so arbitrary ink changes do not require repeating segmentation. Use the
existing validate/write-temp/fsync/rename/rollback pipeline and retain backup.
Do not modify Kodi media artwork or make background network requests.

## Delivery order and acceptance

1. Curate the bundled foreground and ink roles, including internal negative
   space and optical metrics. Review every icon; do not guess brand geometry.
2. Integrate role selection and the three presentation presets into the shared
   app renderer/editor with original artwork and existing layout compatibility.
3. Add conservative raster cleanup only for imported/app-supplied art that
   needs it, including explicit confidence/fallback and remote recovery.
4. Run focused native/QML checks and inspect installed TV-size captures once
   reruns are authorized. Existing deferred runs remain deferred by this plan.

Fixtures must include clean alpha, a white matte, inset rounded plates,
JPEG noise, soft shadows, gradients, disconnected dots, thin strokes, legitimate
white lettering, internal white details, multicolour art and unreadable cases.
Check at32/64/128/512px and on dark/light/pastel/checkerboard backgrounds.
Inspect halos, missing details, jagged contours and colour contamination at
actual TV size as well as enlarged edges. A perfect checkerboard preview alone
does not pass. Screenshot thresholds must not simply bless all white pixels.

Verify D-pad/Confirm/Back, all editor controls, no stranded focus, stable labels,
exact return after launch, cancel/no-write, atomic failure, original recovery,
profile/cache isolation, offline operation and bounded decode/processing work.
New screenshot baselines require agent review. Design-only artifacts earn no
Coding% or software acceptance credit and cause no deployment.
