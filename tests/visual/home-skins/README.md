# Settings-only skin acceptance

Installed release `523f627f0c06211f92b6e43b`, October 16, 2026. All 13 CTest suites passed. Both cases began with packaged default Home in disposable profiles. The helper selected each built-in through Settings, saved a personal skin using the remote keyboard, switched to default, reapplied the saved skin and compared the restored Home document byte for byte. No Home layout file was injected for this route.

The compact component editor and detailed design editor were opened/closed without changing the document. Restore defaults removed the active override and retained the saved skin. Both recreated screens and restored defaults were captured and visually inspected at 1280 × 800. The raw reports leave visual judgment unset; the separate reference-case reviews record the visual assessment and adaptation limits.

| Capture | Evidence |
| --- | --- |
| Skin library | [settings.png](settings.png) |
| Structured design controls | [design-editor.png](design-editor.png) |
| Saved Pastel sky restored | [pastel-restored.png](pastel-restored.png) |
| Saved Cinema lounge restored | [lounge-restored.png](lounge-restored.png) |
| Packaged default restored | [default.png](default.png) |
| Pastel remote-flow result | [pastel-result.json](pastel-result.json) |
| Lounge remote-flow result | [lounge-result.json](lounge-result.json) |

Exact configurations, each default capture, reference images and explicit visual reviews remain in [the pastel case](../home-pastel-reference/README.md) and [the lounge case](../home-reference/README.md). The reference presets use illustrative media/provider cards and do not establish provider access or real viewing history. Wallpaper blur was inspected in the VM; physical hardware performance is not qualified here.

VM evidence: `/tmp/isolde-reference-config-yj_y0vbc` and `/tmp/isolde-reference-config-run1lpai`. Installer log: `/tmp/isolde-skins-install.log`. Household restored active, `NRestarts=0`. Earlier unsuccessful harness navigation is not counted as a passing run; the helper now explicitly selects Home and its initial action instead of assuming Settings forgets its selection.

Repeat with `tools/home-reference-acceptance.py <installed-executable> --case <case-directory> --skin-from-settings --capture-library --inspect`. Capture each announced phase before acknowledging its `<phase>-captured` file. Omit `--capture-library` to skip the library/editor screenshots. The test restores the regular service in `finally`.
