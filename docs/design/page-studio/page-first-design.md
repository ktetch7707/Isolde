# Page-first Studio — revised UX direction

Status: proposed design, 2026-10-19. Based on direct user feedback rejecting the current screenshots; not an implemented interface or a successful usability test. This revision supersedes conflicting toolbar, fitted-canvas, permanent inspector and page-context instructions in the earlier design pack. Persistence, validation, ownership and accessibility gates remain in force.

## Product and diagnosis

Isolde is a studio for designing a personal TV interface on the TV. The authored interface is the workspace. The current provisional shell gives equal prominence to nine tools, shrinks the page into a framed preview, and exposes technical identifiers. It makes editing feel separate from the result. A full-screen Preview does not solve a cramped editing experience.

The revised default is the real page at device size, with a left page menu and temporary context windows overlaid. Opening tools never changes the authored viewport, geometry or saved data. No permanent top action ribbon or right inspector. Desktop uses the same workspace; optional pinned windows are an accelerator, not a different editing model.

## Spatial design

- **Page:** fills the display, including its actual background and navigation. Grid and safe-area guides are off until placement or requested. Selection uses an outline plus a short human-readable name; technical IDs stay in diagnostics.
- **Left page menu:** opening Studio reveals a narrow opaque menu over the left edge: Studio/profile, Add page, named page list, then Add component, Page appearance, Preview, Apply and More. The current page is selected; the designated Home has a house badge with accessible text. Draft changes are named without a permanently verbose footer.
- **Full-view editing:** Edit page collapses the menu to a small Pages control; the page remains full size. Menu restores the page menu from an empty area. Context windows close before another opens. Preview removes all editor chrome; Back restores selection and tools.
- **Context windows:** compact, opaque, readable, anchored near the invoking item and flipped to avoid the selection, safe area and screen edges. Large forms use one temporary side sheet with a collapse-to-view action. Never stack an inspector over a catalog over a page menu. A breadcrumb and Back return to the previous context at its exact focused row.
- **Recovery:** editor colors are an independent accessible Studio palette, adjustable through shared presentation data. An unreadable page cannot hide Apply, Discard or Settings recovery. Ordinary Settings uses Home theme as specified below.

## Pages and Home

Highlighting a page shows its draft behind the menu, without changing the runtime launch page or saving. Confirm opens its context window: **Edit page, Appearance, Rename, Duplicate, Set as Home, Delete page**. Reorder is under More and supports Move up/down, not only dragging. Long names wrap in context headings; page rows have a distinct accessible full label.

Add page opens a name field with on-screen keyboard and Blank / Start from template choices. Blank is the default; no template selection is required. Create enters the empty page with Add component focused. Cancel creates nothing. Duplicate allocates fresh identities, keeps content and explicitly labels links that still point to the original page.

Exactly one existing page is designated Home per profile. Initial packaged Home is that default. **Home is a role, not an undeletable name or fixed page ID.** Delete is visibly disabled for the designated page with “Set another page as Home before deleting this page.” Set as Home previews startup, Home navigation and Settings-color consequences and commits one undoable draft operation. The previous Home becomes an ordinary page. No implicit renaming, deletion or save occurs. Home navigation resolves the designation through validated data.

Deleting another page first lists incoming links and affected navigation entries. Choose a replacement destination or remove those links, then confirm the entire change. Refuse dangling links; Back preserves everything. Deleting the selected component/page returns to a predictable next sibling, previous sibling or permanent Add/recovery control. Undo restores the original selection identity.

## Add component: six understandable families

The catalog starts with six large labeled choices and a small realistic preview, not a flat list of every schema type. Each family exposes content, layout, appearance and navigation controls appropriate to it. Only available capabilities can be added; unavailable sources explain how to configure them without making playback claims.

| Family | First decision | Important controls |
| --- | --- | --- |
| Button | What should it open or do? | Validated page/app/source/action picker; label, icon/art, shape, size, focus style |
| Button carousel | Which buttons belong here? | Add/reorder child buttons; visible count, spacing, horizontal behavior, wrap policy, shared style with child overrides |
| Media shelf | Which Kodi view or source? | Bounded source binding; poster/landscape/list presentation, art crop, labels, item count, empty/offline behavior; no independent media database |
| App bar | Which installed apps? | Manual order or explicit source order, pinned choices, icon/label treatment, density, unavailable-app behavior |
| Image | Which managed image? | Local/managed picker, contain/cover/focal crop, opacity, corners, accessible description, optional validated action |
| Widgets | Which available widget? | Clock/text and other implemented capabilities; widget-specific content, size and shared appearance. Future weather/device support must not masquerade as working. |

After choosing content, the catalog closes and a labeled ghost appears on the full page. D-pad moves the ghost, Confirm places, Back cancels with no node or history entry. An occupied region opens Shift / Swap / Cancel with the affected components previewed; impossible choices are disabled with reasons. After placement the new component is selected; a compact “Edit content · Move · Style” context makes the next steps discoverable.

## Editing without a mouse

Canvas D-pad moves spatial focus. Confirm picks up the focused component; arrows move one cell; Confirm drops; Back restores the origin. The persistent operation label says “Moving [name]” and reports position and validity in words. Menu on a selected component opens **Edit content, Appearance, Move, Resize, Duplicate, Hide, Lock, Remove**. Locked items remain reachable through Layers with an explicit Unlock action. Containers expose Edit contents and a named parent breadcrumb.

Resize has a fixed, visible anchor, D-pad dimensions and a labeled Confirm action. Move to row/column and size fields use the on-screen keyboard. Nested items and carousel children can be reordered with Move before/after. Pointer drag, resize and numeric controls call the same proposals and produce identical data. No long press or hover-only command.

Back cancels a field first, then gesture, then window, then returns to the page menu. Back from that menu offers Continue editing / Discard draft / Review and apply if dirty. Menu opens component actions on a focused component; on empty canvas it opens page tools. A permanent Pages control remains reachable above the first row and on empty pages; returning from a context restores its exact origin. There is no focus trap behind an overlay.

## Appearance that is understandable

Page Appearance has three sections: **Colors, Background, Layout**. Colors starts with coordinated presets and semantic swatches: page surface, component surface, text, secondary text, accent and focus. Editing a swatch opens a labeled palette; advanced values remain optional. Background offers solid, gradient or managed image, with dimming and image focal position. Layout offers screen/scrolling sections, grid, spacing and safe margins with a live change preview. Refuse invalid geometry instead of moving content silently.

Each component offers **Use page style** by default and explicit overrides. Controls include size, spacing, art source/crop, labels/type scale, opacity, corners and focus. Reset removes the override rather than copying today's inherited value. A contrast warning identifies the conflicting roles and offers readable alternatives; essential Studio recovery remains legible regardless of the experiment. Reduced motion and high contrast apply throughout.

Theme resolution: packaged defaults → designated Home page theme → current page override → component override. Pages inherit Home by default, with clear Custom and Reset to Home choices. **Settings and its subpages, dialogs and keyboard always use the designated Home theme**, even when entered from Library. Search, All Apps and Isolde-owned Kodi surfaces use the same Home baseline with documented optional surface overrides; native Kodi skin/settings remain untouched. In Settings, show “Appearance follows [Home page name]” with an Edit Home appearance link.

Changing Home or its colors inside Studio previews consequences within the draft only. Apply atomically publishes the designation, routes and theme together. Discard, profile switches and failures cannot leak provisional colors into live Settings. Background photography is not blindly copied behind settings fields: Settings uses the Home semantic surfaces and readable scrims, respecting the user's background policy.

## Preview, Apply and thoughtful defaults

Preview is the actual full-screen renderer with no selection or grid. Internal page navigation can be tried; external launches/playback/settings writes are intercepted with an honest message. Back returns to the exact page, component, context and camera. Apply opens a compact review: changed pages, new Home, links removed/reassigned and shared-style impact. Apply failure retains the draft and points to the problem; never dismiss the workspace or claim Saved early.

Default template: restrained hierarchy, readable page title/navigation, generous safe margins, a clear featured area and a few intentionally sized shelves/buttons. No unsolicited discovery bars, feeds or autoplay. Detected-source notifications occupy a user-configurable contained slot, never inject rows above authored content or steal focus. Empty pages remain useful and recoverable. Theme choices may be rich or playful; Studio does not force every user's interface into one visual style.

## Implementation slices and acceptance

1. T17: full-size shared geometry; opening/closing overlays does not move any page edge or component. Both generic visual fixtures remain unchanged.
2. T18: page menu, single context stack, Home designation/protection and atomic route/theme consequence preview. Extend the T16 backend through its existing transaction path; do not replace it.
3. T19–20: remote placement/resize/reorder and pointer parity, including empty pages, locked/hidden items and nested scopes.
4. T21: six-family catalog and contextual content/appearance windows. Page color/background controls are core Studio scope, not postponed cosmetic work.
5. T22–23: theme inheritance, reusable content, preview and one atomic multi-page Apply. T27 completes shell adoption; Settings inheritance is required before T24 cutover, so it cannot remain gated exclusively after T24.
6. T24: installed usability evidence. Ask an uncoached representative to create Library, add each available family, style its background, move an item with a remote, set Library as Home, observe Settings inheritance, verify Home deletion protection, reassign Home, delete/remap an ordinary page, preview/discard and Apply/restart. Record confusion, accidental launches, focus losses and blocked journeys; revise until each journey completes without coaching. No screenshot or keyword check proves this gate.

Review screenshots with the menu open, full-view edit, page context, each family, appearance, collision, empty page, dirty exit and Apply failure, using dark, pastel and arbitrary custom themes at TV distances. No clipped controls, unreadable selected item or permanently miniaturized canvas. This is the new direction for implementation; existing provisional UI is not accepted by this document.
