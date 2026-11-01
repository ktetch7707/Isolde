# Customize Home entry correction

The user reported that Customize Home still showed HDMI items alongside a wall of buttons. This was a real integration failure: Settings loaded the legacy `CustomizeHomeView`, whose 32nd action opened “Page Studio (preview)”; named pages used a separate `PageEditor`. Prior Studio screenshots navigated past that screen, so they did not validate the entry the user saw.

User-requested correction takes precedence over starting T22: Customize Home now hosts Page Studio directly. Named-page editing uses the same root-profile Studio session with that page selected. Closing Studio returns to Settings or the invoking page; hiding the editor invalidates the draft. The legacy action grid is retained in `LegacyCustomizeHomeView.qml` only for old operation regression coverage, outside the packaged QML resource list and all normal editing routes. `PageManager` is a compatibility regression component, not a normal page-editing entry.

Saved skins and explicit profile-default restoration remain under More tools. They refuse a dirty draft, so the user must Apply or discard before changing the applied design. Restoration names its profile-wide effect and requires confirmation. This reuses existing skin/reset behavior; it does not claim T22 templates or T23 scoped backup/restore completion.

The new integration regression opens the actual Customize component, creates a draft page, keeps editing after dirty exit, hides/reopens the editor and verifies no layout was saved. It runs inside an offscreen window; a detached-item visibility assertion initially produced a false failure and was corrected to match the app. Existing legacy-operation regressions explicitly identify their compatibility component rather than masquerading as current UX acceptance. The graphical Studio oracle now captures `ENTRY_READY` immediately after the ordinary missing-source/Customize action, before any editor navigation. Both reference oracles use Studio's More menu for saved skins and confirmed restoration.

Existing T21 source changes are included in VM working-tree snapshots; unrelated changes are preserved. This correction does not accept the overall design on the user's behalf or complete T24's usability/performance/offline gates.

## Verification

Canonical run `studio-entry-final-20261020`: `tools/vm-test.ps1 Test`, then `Deploy`, then `Visual -Installed` using that exact RunId. Full native CTest passed 14/14, including the entry/dirty-exit/reopen regression. Source archive SHA-256 `fe312ae9f257280c64e4e6bb2d0839094e702c6a88f388ddd4418aa1cf03c9cf`; manifest digest `b703dede2c4028486093317a13475fb90daec0b44e19bcf0a7eb9bb47bb447ab`. Logs and test/payload identity: `build/vm-oracle/studio-entry-final-20261020/report/`.

Installed release `d8c9df6d050177e70eacfe2e`; previous `237dd5ba7b8b534fc4ec88f6`. Studio's actual entry plus editing/history/collision/resize/Apply/nested-inspector journeys passed (12 captures), and both lounge/pastel skin round-trip and recreation/reset pairs passed (four captures). All 16 images were viewed. Entry shows the left page menu over the actual page; the legacy button wall is absent. Reference recreation images retain the established generic renderer composition. These are isolated test profiles, not edits to Household.

Capture directories under that run: `visual/studio/attempt-20261020-210954-255/`, `visual/lounge/attempt-20261020-211119-814/`, `visual/pastel/attempt-20261020-211205-909/`. First-entry evidence is `studio-entry.png`, captured before any Studio tool navigation.

Each graphical run restored the normal service active/running with zero restarts and preserved the normal layout SHA-256 `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`. Final service PID `1239857`. Release links remained unchanged throughout graphics. Physical remote hardware, disconnected Generator Test and representative usability approval were not claimed.
