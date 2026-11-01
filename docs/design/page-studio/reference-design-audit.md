# Reference-driven Studio design audit

Date: 2026-10-20. Planning analysis of the two user-supplied reference images and the current Page-first Studio specification. These are proposed requirements, not implementation evidence. Existing safety, profile, ownership, offline and atomicity requirements remain unchanged.

## Product promise and boundary

A user should be able to reconstruct the visual composition of a reference using editable components, real data bindings and supported actions, entirely on a TV. A reference is guidance, not a background screenshot pretending to be an interactive interface. No automatic screenshot-to-components conversion is promised. Arbitrary executable websites, unavailable services, new hardware capabilities and every possible visual effect are outside that promise. Unsupported effects must be identified, not silently approximated or advertised as supported.

Photo 1 is a landscape TV composition. Photo 2 is a portrait capture of an interface within phone/chat chrome. Its status bar, chat composer and surrounding controls are not Isolde components. Reconstruct the dark interface itself, and deliberately adapt its composition to a TV viewport rather than stretching the whole photograph.

## What the references actually require

| Reference detail | Authoring capability | Clarification required for implementation |
| --- | --- | --- |
| Pastel sky and mountains | Full-page managed background image with focal crop and optional dim/tint | Background is independent of child opacity; crop must be adjustable on the remote |
| Circular HDMI/source buttons | Shape preset, icon, per-item color and focus state | Width/height alone does not define a circle: offer preserve aspect ratio; focus ring must not move neighbors |
| Labels underneath circles | Independent label placement and styled label surface | Below/inside/overlay placement, wrap policy, spacing and pill background are separate controls |
| Favorite Apps translucent panel | Container surface, heading/icon and child app bar | Surface opacity, child opacity, border, corners and optional bounded blur are independent; unsupported blur has an explicit fallback |
| Recently Opened strip | Data-bound cards, separate image/title/subtitle and section action | Mixed games/music/photos/history cannot be presented as existing Kodi capability; each adapter must be verified, otherwise unavailable or clearly labeled sample |
| Logo, Search/Settings capsule, clock/date | Editable header container with independent children and alignment | No permanently reserved uneditable header that prevents recreating the image; guaranteed editor recovery is outside authored content |
| Dark navigation tabs | Page-navigation component, selection state and page links | Distinguish selected page, keyboard/remote focus and pressed state; selecting a tab uses validated destinations |
| Wide castle hero | Image component with aspect ratio, focal crop, rounded mask and optional overlay text/action | Branding visible in the source artwork is part of the supplied image, not a required separate Isolde control |
| Continue Watching cards | Media shelf with image, title, optional progress and supporting text | Kodi owns progress and media; rearranging or restyling does not create progress or fetch a parallel library |
| Four source cards and guide banner | Button group plus wide icon/text/action card | Same button primitive can produce both; nested text/icon layout and focus treatment must be configurable |
| Both images' visual rhythm | Consistent type roles, alignment, gaps, padding and section heights | A coarse page grid alone cannot express these details; use child layout and style controls, not dozens of spacer components |

## Missing design decisions — proposed resolutions

### 1. A composition model underneath the six catalog families

Keep Button, Button carousel, Media shelf, App bar, Image and Widgets as the friendly entry points. Add a clearly named **Section / Group** layout tool alongside them. A section owns a background, heading, optional section action and child layout. Users need not understand a scene graph. Select an item, choose Group with…, then enter Edit contents; a breadcrumb always identifies the parent. Reusable sections can be saved later.

Widgets must include standalone text/heading and clock/date presentations. A button supports icon-only, text-only, icon+text and image-backed presets through one schema. App bars and media shelves are editable repeated-card presentations over data, not unrelated renderers with incompatible styling. The header is an editable section, not fixed chrome. Layout is still bounded validated data, never custom code.

### 2. Layout power without coordinate overload

Retain the bounded page grid for top-level placement. Inside sections expose **Row / Column / Grid**, alignment, distribution, gap and padding. Separate the selected component's outer box from its inner content. Offer preserve aspect ratio, fit/fill and label position before advanced measurements. Width/Height show their unit and effect: page-cell spans outside, permitted child-grid spans or relative/fixed design-space dimensions inside. Never overload the same number with an unexplained meaning.

Alignment tools: align left/center/right and top/middle/bottom, equal size, equal spacing, and group movement. Snapping guides show edges, centers and spacing only during manipulation. Move before/after is available for ordered layouts; top-level collisions still require Shift/Swap/Cancel. Do not turn Shift into an automatic hidden reflow policy.

The existing 4–16 by 3–10 grid cannot alone promise pixel-level reproduction of arbitrary screenshots. Precision comes from bounded internal layout and explicit styling. Advanced offsets/measurements, if needed, require a separately validated design-space schema and remote control; no ad hoc QML pixel constants. Record any reference detail the generic model cannot represent before claiming fidelity.

### 3. Organize properties by the user's question

The selection context stays **Move · Width · Height · Edit component**. Edit component opens four consistent groups:

- **Content:** text, icon/artwork, live source and On select link.
- **Look:** style preset, colors, shape, type, label position, background and focus treatment.
- **Layout:** inner alignment, gap/padding, ratio and child arrangement.
- **More:** accessibility, visibility/lock, advanced state rules and diagnostics.

Only relevant controls appear: a decorative image has no media sort controls; a shelf exposes item/card styling separately from its outer section. Choose **Edit this card style** rather than trying to select a transient live media item as a persistent layout node. Show inheritance and Reset next to the relevant value. Defaults produce a coherent result before any advanced control is opened.

### 4. Style states, typography and surfaces

Define shared presentation data for normal, focused, selected/current, pressed, disabled and unavailable states. Remote focus must not be confused with current navigation selection. Focus enlargement draws in a reserved visual overflow envelope and does not shift layout, cover labels or disappear under clipping. Reduced motion substitutes stable emphasis.

Type controls need readable role presets (page title, section title, card title, supporting text), bounded size/weight choices, alignment, wrap/line count and truncation behavior. Use packaged fonts initially; arbitrary font import is not implied. Label placement and its backing surface are independent of the card artwork. Expose surface fill/tint, opacity, border, radius and restrained shadow through shared models. Blur is optional and capability-gated; specify bounds and performance evidence before shipping it. A translucent panel must not make all descendants translucent.

Page tokens supply useful defaults; per-section/per-component overrides allow the multicolor pastel buttons without severing all inheritance. Save a style and Apply style to similar items are more useful than manually repeating twenty fields. Explicitly preview whether a shared change affects one item or every linked instance.

### 5. Reference-image workflow

Under page More, offer **Design reference**: import a managed local image, crop away irrelevant screenshot chrome, then choose a temporary overlay or small reference window. Opacity and Show/Hide are editor tools, independent of the page background. Remote controls can move the reference window away from the selected region; one-button hide restores a full unobscured screen. A reference never becomes runtime content or a focus target, and never appears in the skin export. Reference state is editor-only and profile-scoped; any retained image is bounded and private. Cancel/discard follows the asset staging contract.

Start with reference beside the current design or overlay, then place sections and add children. Offer editable starter patterns—circular launcher, glass panel, media shelf, hero banner, navigation strip—assembled from the generic model. These are starting styles, never reference-name branches or flattened images. Image import extracts no actions or data bindings automatically. Color sampling is optional, with a remote target and confirmation; sampled colors still need readable text/focus checks.

### 6. Live content and unavailable states

Separate **what supplies items**, **which supported view/order**, **how a card looks**, and **what activation does**. Display a source's real capability and ownership. Manual launch buttons can point at supported actions; media shelf items retain their supported media behavior. Do not offer scripts or invented sources to imitate a reference.

Editing freezes the bounded data snapshot during gestures; source refresh cannot move the selected item. Offer clearly labeled sample content for designing empty/unconfigured shelves, without persisting fake media/progress. Configure empty/loading/error/offline appearance and whether an empty shelf reserves its space or collapses according to explicit page policy. Sample state cannot be mistaken for ordinary runtime data. Favorite app logos require actual installed-app/managed artwork; the reference does not establish entitlement.

### 7. Aspect ratios and scrolling

The actual device remains the default edit viewport, with no mandatory Preview button. Page setup chooses one-screen or scrolling sections. A portrait reference needs an intentional landscape arrangement; typography and card proportions must not be stretched. Other display aspects are optional editor checks using the same renderer. Preserve aspect/focal points, use bounded layout rules, and report overflow before applying. No read-time rewrite and no silent fit that changes authored placement. Multiple aspect-specific compositions are a possible future schema extension, not an implicit claim of this plan.

### 8. A beautiful editor needs its own design specification

Ship one coherent Studio appearance preset: readable typography roles, safe margins, compact context windows, restrained surfaces, clear active/disabled/error states and consistent control spacing. Encode these in shared configurable presentation defaults. Page colors can be experimental; Studio's selection and recovery remain readable over bright photography and dark surfaces.

Selection has a named outline; editing has a distinct operation label; unapplied changes have a quiet draft indicator. No permanent dense toolbar, wobble animation, raw IDs or full-screen property lists. A single context window must avoid the item being edited; if no safe position exists, collapse it to a small operation control and provide Show controls. Numeric fields do not hide the result behind the keyboard. First-time contextual hints disappear after use and remain available through Help.

## Acceptance: prove expressive power and ease separately

1. Recreate both references using the same renderer and editable data, with equivalent available source bindings. Keep an explicit list of unsupported artwork/effects/data; no fixture-specific code. Exclude phone/chat chrome from Photo 2.
2. Review composition, relative sizing, spacing, crop, labels, surfaces, type hierarchy, focus states and full-device legibility—not merely whether a similarly named component exists. Record agreed tolerances before implementation acceptance; do not invent a universal pixel threshold for different aspects/assets.
3. An uncoached beginner creates a page, adds a circular image-linked button, changes width/height, groups buttons over a panel, adds a media shelf, edits card style, sets a background and applies. A second session reconstructs the dark hero/shelf/source/banner structure. Record completion, assistance, mistaken actions and focus losses. A failed journey requires redesign, not documentation of a hidden shortcut.
4. A practiced user performs alignment/distribution, child editing, multi-select, duplicate/style reuse, reference comparison, link repair and undo/redo with a remote. Repeat equivalent pointer commands and verify canonical data parity.
5. Exercise long labels, missing art, no media, unavailable apps, disconnected sources, arbitrary bright/dark colors, reduced motion, large type, full grid and failed Apply. Keep original bytes and exact focus on cancellation/failure.

## Backlog ownership and order

- **T17:** shared geometry, section/child layout contract and ratio/overflow behavior; resolve representability before building more controls.
- **T18:** selection/parent scope, contextual-window placement and full-view editor shell.
- **T19–T20:** remote/pointer movement, grouping, alignment/distribution and nested editing through the same atomic commands.
- **T21:** six-family catalog plus Section/Group tool, type/label/surface/state controls, card-style editing, links and honest data states.
- **T22:** editable starter patterns, shared style reuse and private editor-only Design reference workflow; preserve export privacy.
- **T23:** readable preflight/error recovery, optional Try interaction and atomic Apply; reference/selection tools never enter runtime layout content.
- **T24:** reference reconstruction, installed visual review and uncoached beginner/practiced-user evidence. Remaining product/hardware gates still apply.

No new task IDs or completion credit are introduced by this design audit. Implementation must assess these requirements within the existing slices rather than treating this document as delivered functionality.
