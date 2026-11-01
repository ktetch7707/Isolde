# Studio selection and switch slice

User-requested T21.12 selection work plus a T21.11 control-audit slice, 2026-10-21. This explicitly selected scope continues the pending appearance-slider work. It does not close the earlier T19/T20 gates or claim completion of T21.

## Application changes

- Placed cards, nested groups, library components, composed cards and legacy tiles use their existing Home selection renderer while their ID is selected in Studio. Runtime selection conditions remain in the non-editing branch. Context and inspector focus do not clear component selection.
- Removed the heavy rectangular Studio borders around selected components and proposal cards. Hit targets, resize handles, placement guides and movement validation remain. Proposal replicas highlight the selected component using its actual renderer.
- Placed components with low authored opacity or disabled focus get a transparent recovery outline outside their body's opacity. HomeCard and OrbCard expose the actual rendered surface geometry, so an orb's recovery outline follows its diameter rather than its grid cell. Authored artwork opacity and saved document data remain unchanged.
- Existing ordinary boolean properties use labeled switches with themed tracks/thumbs and accessible checked state. Pointer clicks and remote commands share the existing validated edit/Undo path. The page-scrolling checkbox keeps its separate preview transaction.

## Verification status

Added `tests/studio/tst_selection.qml`: authored orb color/width/shape, transparent recovery, selection persistence through context/inspector, rounded selection and pointer/remote switch parity with exact Undo and no disk write. Its separate `studio_selection` CTest registration retains a 30-second timeout; it is included in the focused `studio` aggregate. The existing 13-scenario `studio_quick` registration remains separately bounded at 30 seconds.

`pastel-live-20261022-full-c` passes all 18 CTest registrations, including all four selection/collection scenarios and the separate 13-scenario controls registration. All seven QML captures and both standing visual save/reset pairs were opened and reviewed. Evidence: `build/vm-oracle/pastel-live-20261022-full-c/`. The original preflight failures remain preserved; their guest capacity blocker was resolved under explicit/standing VM cleanup authorization.

The switch regression found a real ListView focus bug: model replacement reset the visible row to index zero despite the inspector retaining its selected field. The inspector now restores that binding and scroll origin after reset. Tests assert canonical Undo, field origin, pointer/remote parity and no disk write. See `studio-live-carousels-2026-10-21.md` for installed release and follow-up startup recovery status.
Remaining acceptance includes ancestor-opacity/nested clipping, all section/header selection paths, both complete fixture reconstructions, reduced-motion checks, unavailable content and uncoached usability. T21.11/T21.12 remain open; percentages remain T21 36%, Overall 35% across 24 active tasks.
