# Home reference acceptance

[reference.jpg](reference.jpg) is the unchanged user-supplied reference, saved October 16, 2026. Keep this original in the repository. It is a visual acceptance fixture, not a bundled application background or a screenshot to render as the interface.

The requirement is a **convincing recreation through configuration**. Matching the colors while retaining generic launcher tiles fails. Implement reusable configuration capabilities; do not create a special screen hard-coded for this image.

Recreate the Isolde content inside the image. Exclude the surrounding phone status bar, ChatGPT buttons, composer and scroll arrow. Adapt to a landscape television while preserving the visual hierarchy, relative proportions, alignment and generous space. The portrait reference is not a demand for phone-shaped TV content.

## Visual acceptance criteria

- A restrained navigation row with Home, Movies, Television, Music, Games and Kids; active and inactive states are distinct. Names, order, visibility and destinations are configuration.
- A wide, rounded image banner with controlled crop/aspect ratio and an optional small overlaid label.
- Continue Watching with a clear heading and secondary sentence; three landscape artwork cards, titles below the artwork and a thin resume indicator below the focused title. Rose selection outlines follow the artwork instead of surrounding a generic text tile.
- Your Sources with four compact cards, centered line icons and labels, independent card/background accents and consistent spacing. The second source can have its own muted-rose surface.
- On Tonight with a compact full-width guide card: icon at left, title/subtitle, trailing chevron and balanced padding.
- Near-black background, charcoal surfaces, muted-rose accents, readable white/gray type, consistent rounded corners and clear hierarchy. No clipping, accidental blank areas or huge clock displacing the design. The agreed default TV arrangement fits one screen.
- Every visible part is a real component, not a flattened picture. Directional navigation, focus restoration and configured actions work. Visual fixture content may be explicitly synthetic; real resume data stays Kodi-owned. Unavailable guide/capture backends are never represented as operational.

Exact movie art and the castle illustration can be supplied as local fixture assets when available; absence of those exact assets does not excuse wrong geometry, missing components or the wrong arrangement. Keep the supplied reference intact and identify any substitute art in the run record. Judge the full composition side by side, not only individual colors or automated pixel scores.

## Repeatable procedure

Run this acceptance whenever Home composition, navigation, media/source/guide cards, appearance controls or their configuration changes. Update the configuration fixture to use the newly supported capabilities, without editing QML just for the test.

1. Start the installed build in an isolated profile and record its release and reference SHA-256. Preserve existing personal settings.
2. Apply the reference configuration using supported options. Capture the actual Home at a recorded viewport, with editing overlays closed. Check remote navigation as well as visual fidelity.
3. Compare the capture against the reference and record each criterion as pass/fail with remaining gaps. A successful launch or a successful reset does not mean the visual match passes.
4. Invoke Restore defaults, return to the top of Home and capture a second screenshot. Verify that the test override is gone and the packaged default is displayed.
5. Restore the normal service. Retain both screenshots, the exact attempt configuration and result metadata. Update this directory's last-run artifacts only with actual captures.

The existing VM helper is `tools/home-reference-acceptance.py <installed-executable> --inspect`. It loads [recreation-config.json](recreation-config.json), copies local reference artwork into the isolated test profile and records the reference hash. It prints `CUSTOM_READY <evidence-dir>` and waits for `<evidence-dir>/custom-captured` after the first screenshot. It then exercises the component editor, resets using remote keys, prints `DEFAULT_READY`, and waits for `default-captured` after the second screenshot. Both waits are bounded and normal service restoration runs in `finally`. The helper leaves its visual verdict unset; an explicit visual review is required. Do not turn a process exit code into an automatic visual pass.

## Last completed run

**Visual recreation: passes as a landscape adaptation. Default reset: passed.** Installed release `523f627f0c06211f92b6e43b`, October 16, 2026, 1280 × 800 VM display. The actual images are [last-attempt.png](last-attempt.png) and [last-default.png](last-default.png); exact settings are [last-attempt-config.json](last-attempt-config.json). The explicit assessment and limitations are in [last-review.json](last-review.json). Banner/poster artwork is cropped from the retained local reference; all other interface elements are rendered independently. Thirteen automated suites passed. VM evidence: `/tmp/isolde-reference-config-run1lpai`. Household restored active with zero restarts.

This is a visual component pass, not qualification of live Continue Watching, HDMI switching or guide schedules. The fixture contains synthetic labels/progress and setup actions; live data remains Kodi/backend work. Restore defaults returns to the packaged legacy grid. Settings now exposes the detailed presentation controls and all three selectable designs; saved skins round-trip through the skin library. A new live-data first-run preset remains open.

The original failed run remains preserved as [baseline-failed-attempt.png](baseline-failed-attempt.png), [baseline-failed-default.png](baseline-failed-default.png) and [baseline-failed-config.json](baseline-failed-config.json), from release `0bf049b4a83a64ae665985e4`.

The latest run used --skin-from-settings: it began with default Home, selected Cinema lounge, saved a personal skin, switched to default and restored the saved skin entirely through remote input. Reset preserved the saved skin. See [skin acceptance](../home-skins/README.md).
