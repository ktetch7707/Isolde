# T21.11: visual appearance sliders

Continuation of the user-selected interface-control improvements. This is a scoped implementation of the control audit, not completion of the broader T19/T20/T21 gates.

## Implemented

- Opted-in numeric appearance fields now open a compact live slider instead of a keyboard: opacity, dimming, blur strength, corners, border/focus width, text/icon/art sizes, spacing/padding and supported page dimensions. Discrete collection counts and technical fields keep their existing controls; they are not silently converted to inappropriate sliders.
- Presentation is data in `studio-defaults.json`: each existing numeric field opts in through `control: slider`, with optional display units/scale and named presets. Existing bounds/steps remain authoritative. Normalized opacity/dimming values display as percentages, including optional exact input. Corners expose Square, Soft and Rounded choices with remote/pointer access.
- Opening preserves existing custom values, including values between preset steps. Explicit adjustment is the only way to change them. The actual full-size runtime draft previews the result while the context stays within the viewport and avoids the selected component where possible. Entry typography/palette are held stable for the control itself.
- Pending previews coalesce, remain private across the optional exact-value keyboard, and validate through the same session operations as other inspector changes. Done accepts once; Back/Cancel drops queued updates and restores exact draft/history/disk. Reset removes the override rather than copying a theme value. Apply remains the save boundary.
- Null selection/model transitions during proposal acceptance are handled by the shared effective-value lookup. The panel loads only while needed. No new dependency, stored-layout migration, profile store or media provider is introduced.

## Verification so far

Three new QML scenarios cover percentage endpoints, pointer/remote canonical parity, preview/cancel/no disk writes, exact-value bounds and keyboard cancellation, reset cancellation, one-step undo, remote corner presets, existing off-step value preservation, and actual panel containment. The existing native inspector regression is updated to require a private opacity preview before explicit acceptance.

`studio-sliders-20261021-a` exposed a genuine Loader sizing problem: its Rectangle was stretched to viewport size. A full-size wrapper now contains the independently sized context. The failed captures were opened and rejected. `studio-sliders-20261021-b` exposed an inherited-value lookup dereferencing a transient null item during proposal replacement; that was corrected, not hidden by weakening warning checks. `studio-sliders-20261021-c` passes all **13 QML scenarios**. Current corners/dimming captures were opened and reviewed; current color captures are byte-identical to the already-reviewed color delivery images.

Evidence: `build/vm-oracle/studio-sliders-20261021-c/report/`, including `ctest.log`, `LastTest.log` and `studio-test-scalar-{corners,dimming}.png`. The native suite and standing fixture verification are **pending**, so this is not marked delivery-complete or committed/deployed.

## Integration checkpoint (2026-10-22)

Full run `pastel-live-20261022-full-c` passes 18/18 registrations, including the native inspector preview/accept/reset regression and all 13 controls scenarios. All seven control/selection images and four visual save/reset captures were opened and reviewed. Release `a72852697c1f50e138161a63` was installed after those checks. Guest capacity cleanup was approved and completed; routine guest maintenance now follows the standing boundary in `AGENTS.md` and `vm-oracle.md`. See `studio-live-carousels-2026-10-21.md` for the combined delivery and remaining installed checks. No broader task completion is claimed.
Accounting is unchanged: T19 83%, T20 80%, T21 36%; 24 active tasks, round(849/24) = **35% overall**. Remaining steppers, segmented choices, crop/content pickers, linked spacing controls, broader corner/typography presets, mixed states and uncoached usability remain open.
