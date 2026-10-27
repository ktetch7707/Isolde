# Studio controls implementation checkpoint — 2026-10-27

The user authorized implementation and delegated VM testing to another agent. This checkpoint records source changes and host checks, not native, installed, visual or usability acceptance. T20 remains 80%, T21 remains 36%, and Overall remains 32% (860 / 27 ACTIVE tasks, rounded). No broad Scope box is newly checked.

The preceding pointer batch is recorded in [studio-pointer-boundaries-2026-10-27.md](studio-pointer-boundaries-2026-10-27.md).

## Implemented source

- A shared visual app picker provides All installed/Selected modes, checked app tiles, Select all/Clear, explicit selected order, pointer/remote ordering, cancellation and unavailable retained members. Both Add and Edit admit desktop, web and Android apps. New missing memberships are refused; existing missing memberships can be retained or removed. Selection is bounded at 128 and refuses overflow without truncation.
- Button carousels retain ordinary stable authored child identities for Edit contents. Runtime uses the shared collection renderer with scoped More buttons, a virtualized full gallery, exact launch/return state, unassigned-link feedback and multiple-row grids. Row capacity is independently configurable; row membership supports 128 buttons, while ordinary grid dimensions remain bounded at 16 columns. Schema validation refuses non-button members and oversized collections.
- Shape and Text are first-class catalog entries. Shapes support rectangle, rounded rectangle, ellipse, pill and divider; real containers remain separate. Shapes are decorative in runtime focus and selectable in Studio. Shared presentation now includes validated solid/gradient fills and independent visible labels and text bodies. Editor names, captions and accessibility names remain separate. Elliptical selection follows the actual visible bounds.
- Replace in place is a validated proposal for compatible leaf components and empty containers. It preserves identity and placement, lists incompatible properties before acceptance, defaults to Cancel, and accepts as one draft/history transaction. Collections and nonempty containers are explicitly refused.
- The actual header can be selected directly, reached from Layers and opened from Page appearance. A shared validated composition header model controls ordering, title/logo, clock/date formats, visibility, dimensions, styles and control display. Runtime and preview use the same model. Packaged defaults preserve legacy presentation; stored documents are not rewritten on startup.
- Undo/Redo have a compact remote-operable history row. Layers, Saved designs, Editor settings and secondary Try interaction have named locations. Component, page and Studio colors use the shared visual picker; legacy color entry points now route there as well.
- Reset currently restores **the current page's packaged defaults** as one undoable draft edit. It preserves other pages, Home designation, apps, accounts, Kodi and Studio colors. It makes no applied write until Apply. The dialog states its scope and defaults to Cancel. This is not the complete T23 whole-interface restore/version workflow.
- A shared searchable action picker filters registered choices and bounded packaged aliases, provides data-defined categories, and retains its search/category through target/media subdialogs. It only selects existing actions; native action validation and the existing Kodi picker still own execution/identity boundaries. Back preserves the original field.

## Verification and handoff

The host has Node but no CMake/CTest/Qt test tools on PATH. No host toolchain was installed. No VM operation was started.

Host checks pass:

1. `node build/studio-pointer-boundary-20261027/host-smoke.cjs`: actual pointer-method checks, zoom conversion, refused targets, remote recovery, Android eligibility and generic Pastel preset/fixture data parity.
2. `node build/studio-finish-20261027/verify-source.cjs`: 243 extracted QML JavaScript functions and 68 handlers compile; actual shared app/action/carousel methods pass their bounded smoke cases. Packaged settings parse within 64 KiB and header geometry/defaults satisfy the checked bounds.
3. `git diff --check`: no whitespace errors.

Durable source hashes and check scope are in [quality/studio-controls-host-20261027.json](quality/studio-controls-host-20261027.json). These JavaScript checks do not type-check QML, compile C++, execute Qt input, or establish visual acceptance.

Native/QML regressions were added to `tests/tst_components.cpp` and `tests/studio/tst_{editor,gestures,selection}.qml`. New native cases are included in the focused `studio_unit` CTest selection:

- `studioHeaderValidationAndDraftRollback`
- `studioPageDefaultsAreDraftOnlyAndScoped`
- `studioShapesLabelsAndReplacementAreAtomic`
- `studioButtonCollectionsAreBoundedAndAtomic`

The testing agent should read `docs/vm-oracle.md`, use the bounded runner, run the Studio suites and full CTest, stop at the first named failure, then review both generic reference recreation/reset pairs and actual remote/pointer journeys. Include All/Selected apps with Android/removal, button overflow and launch/return, shape bounds/labels, replacement cancel/undo, header recovery, colors and page reset with unchanged applied bytes.

## Remaining gate scope

GIF animation through the bounded managed-artwork pipeline is still unimplemented; accepting a filename extension would not satisfy it. Recently viewed defaults to genuine Kodi history; no external app-history adapter/account capability has been selected or proved. The user requested clarification of that distinction, not a provider selection. Keep provider integration open without scraping browser history or inventing results.

T21 also retains its fixture/design, complete control-disclosure and uncoached discovery gates. T22 templates/tokens/reuse/portability and T23 complete Apply/versions/restore/health remain open; this checkpoint does not bypass their dependencies. VM testing is delegated, not counted as a pass. No software or release completion is claimed.
