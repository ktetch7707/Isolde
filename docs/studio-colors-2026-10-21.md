# T21.10: Page appearance color handling

The user's explicit color request selects this scoped T21.10 slice ahead of the still-open broader T19/T20 work. It does not close those dependencies or the complete T21.10 gate.

## Implemented

- Page color fields show role labels, visible swatches and Theme/Custom state rather than requiring hex entry. The same picker serves existing inspector component/section/state/background color fields through their existing validated operations.
- One context contains Theme, eight named Presets, eight bounded Recent colors, and Custom. The hue/saturation wheel works by pointer drag or remote adjustment; brightness has a separate labeled slider. Before/After samples, a visible wheel target and contrast estimate stay visible while the page retains full viewport geometry.
- Wheel pointer values use stable one-degree hue/one-percent saturation increments; remote adjustment uses five-degree hue/five-percent saturation steps. The same values serialize identically. Brightness is independent of hue/saturation. Exact six-digit RGB input remains optional through the existing remote on-screen keyboard, with invalid input refused.
- Changes use `StudioSession.previewCommand`, and Studio displays its proposal model. Rapid input coalesces pending preview rebuilds; acceptance flushes the final candidate and accepts once. Cancel drops queued updates, removes the proposal and restores the original draft. Nothing saves until the existing Apply operation.
- Reset to theme removes the override using the same proposal/accept transaction. It does not copy the effective parent color into an override. The field origin, selected component and inspector return selection remain available after the picker closes.
- Recent colors update only on accepted changed values, deduplicate, and are limited to eight in the active inspector/session. Session replacement clears them; there is no new cross-profile preference store. Picker chrome uses the entry palette so changing page text/focus colors cannot make the picker itself unreadable.
- Page background/text contrast uses both solid gradient endpoints; other text contrast uses the relevant surface. This is explicitly a solid-color estimate, not proof of contrast over wallpaper/artwork.
- Packaged named presets are data in `config/studio-defaults.json`; `HomeLayout::studioOptions()` checks preset shape, count, name and opaque color validity. No stored layout migration or new runtime dependency is introduced.

## Verification

Four new executable Qt Quick scenarios cover named swatch live preview/cancel, one accepted edit/undo, distinct actual wheel positions (red/cyan), pointer/remote canonical equality, pending-preview cancellation, override removal/undo, recent acceptance rules and invalid/valid optional exact input. Existing six Studio scenarios remain registered. Actual page-model replacement is observed through the editor's current binding; tests do not retain an obsolete preview model.

The first pointer test exposed first-layout timing and then a one-channel axis drift caused by integer scene coordinates. The test now waits for displayed geometry; wheel input has stable coordinate quantization. Assertions still require exact expected colors and exact document parity. Palette initialization warnings were corrected. Coalescing initially exposed a test retaining the prior model object; it now observes the live editor binding without weakening the expected preview color. Superseded captures were opened and rejected where preview had not settled.

`studio-color-final-20261021` passed the new color cases but its broad navigation registration exceeded the unchanged 60-second deadline. Picker creation was moved behind an on-demand Loader; the timeout was not raised. Final review also added a transaction flag independent of the current subdialog and a regression for opening Exact color with a queued preview, preventing accidental draft commits while typing. `studio-color-verified-20261021` then passed all three focused suites.

Final full run **`studio-color-delivery-20261021` passed 17/17 CTest registrations in 144.69 seconds**, including all ten QML scenarios in 18.50 seconds and all 19 runner safety tests. Archive SHA-256: `5990bf029cae6f663772010f298ea5ce38b58b1e8957479c2f5b2e8c196171dc`. Source digest: `34d034ce4b07930f9f1b3972fe1e380b6b61bb9f807ac4796b956d45289e0c53`. Final logs/JUnit are in `build/vm-oracle/studio-color-delivery-20261021/report/`. The accepted preset and cyan-wheel images there were both opened and reviewed: full page remains visible, panel fits, real preview and After sample agree, names/controls are legible and low-contrast feedback updates. Earlier captures were also opened; stale preview frames are superseded by these settled captures.

The full native gate preserved the installed service identity exactly: active/running, PID 1405231, NRestarts 0, release `6d558525d9ead04d09f31aa9`. The staged lounge and pastel recreation/reset cases both passed, and all four captures were opened and reviewed against their retained reference adaptations. Lounge evidence: `visual/lounge/attempt-20261021-143536-077/`, guest `/tmp/isolde-reference-config-h8eawtkm`; pastel: `visual/pastel/attempt-20261021-143622-428/`, guest `/tmp/isolde-reference-config-5v3ii1bh`. Both preserved normal `home-layout.json` SHA-256 `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`. The guardian restored the existing installed app active/running, PID 1465160, NRestarts 0, on the same release. No Deploy was issued. All seven code/config/test files were checked against the final frozen manifest. This review accepts the visual regression slice, not uncoached T24 usability.

## Scope and release status

This slice retains the current opaque RGB color schema. Opacity continues to be a separate existing presentation property; this does not add a page alpha-channel model or falsely expose unsupported transparency. Persistent profile-local recents, comprehensive mixed-selection treatment, dedicated Studio chrome adoption and uncoached/hardware usability remain open parts of the broader color gate. It is not a claim that all T21.10 requirements are complete.

The installed VM release stays unchanged unless a later explicit Deploy checkpoint is requested. Testing uses the isolated cached runner; the full native gate can stage a payload without installing it. Preserve the unrelated working-tree changes to `docs/components.md`, `docs/design/page-studio/feature-map.md`, `src/home/PageData.h` and `evidence/`.

Accounting: T19 83%, T20 80%, T21 36%; Overall round(849/24) = **35%**, 24 active tasks. No gate is checked off for a partial implementation.
