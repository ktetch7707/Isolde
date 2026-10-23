# T20 explicit group destinations — 2026-10-23

Move → Options now exposes **Move into group…**. Targets use authored group names; current scope is identified, and locked/hidden groups, their descendants, the selected group and its descendants are excluded. The shared placement boundary still rejects invalid destinations, including forced cycles and locked ancestors.

Choosing a target changes only the visible candidate. Remote directions or pointer grid-cell selection choose its position; Confirm / Place here accepts one shared placement transaction. Options and Cancel remain visible for pointer users. Placement never infers reparenting from incidental overlap with a group. The action panel uses shared Studio appearance and sits opposite the candidate. No motion is required to understand the target or affected components.

Back restores the source group, exact selected component and viewport. Full destinations preserve the original document and show the refusal; pointer Place is unavailable. Changes remain private until ordinary Apply. Group selection in a scrolling-section layout follows the target's actual containing section.

## Verification

`t20-group-targets-20261023-a` failed because pointer input was issued before the real menu delegate existed. Its diagnostic captures were opened. The test now waits for that concrete delegate and settled position, retaining actual pointer clicks and all semantic assertions.

`t20-group-targets-20261023-b` passes the three destination scenarios in 8.74 seconds. Its target-preview capture was opened; subsequent work enlarges the fixture destination and adds actual pointer cell selection, with remote/pointer canonical parity at a nonzero destination column. Run c passes all seven focused Studio registrations in **107.41 seconds**; source digest `0f852a76d6ac75b9c3634d9534fcea5aa64e0261c1775f3c8eb6e65771246b0d`. Its target capture was opened and leaves the selected component visible beside the named action panel.

`t20-group-targets-20261023-full-d` is **not a passing checkpoint**: the conversation's host tool session disappeared, closing its output pipe during CTest. Runner Status records `[Errno 32] Broken pipe`; the retained CTest log stops during studio_grid after the first three registrations passed. The guardian stopped its owned child and retained the normal active service/release. Retrieved its existing report through the bounded transport helper without rebuilding or changing the result. Full-e uses the same bounded runner launched by an independent hidden host PowerShell process with redirected workspace logs (`build/t20-group-targets-full-e-host.*`) to survive conversation-session closure; full verification is pending.

Full-e passes **21/21 CTest registrations in 198.03 seconds**. Source digest `2664e9e1de7f1fb356ca9717f1a8b25ab629161a1aad2fd2eaf1366c38f6ace1`; archive SHA256 `3e9e452128efda46fb6150cdf4d3cb93fe980562f7b306cfe071d1c64dfe4b63`. All twelve offscreen captures match run c byte-for-byte; its eleven unchanged control captures also match the reviewed page-grid full-d checkpoint. The named group-move image was opened separately. Normal service/release remain unchanged. The staged graphical journeys are checked separately before committing.

Existing unrelated worktree edits are outside this slice. No deployment, task checkbox or full visual acceptance is claimed. T20 remains 80%; Overall round(866/24) = 36% across 24 active tasks. The complete mixed-size/decorative/reduced-motion input matrix and installed acceptance remain open, including Pastel's populated reference gate.

## Remaining movement coverage audit

| Requirement | Current evidence | Still required |
| --- | --- | --- |
| Named cross-group targets | `tst_destinations.qml`: remote/pointer nonzero-cell parity, full target, locks/ancestors, cycles, exact cancel/undo | Installed desktop/remote journey; cross-section target preview |
| Pointer threshold and held-drag cancellation | `tst_gestures.qml` actual press/move/Back/release | Complete policy matrix below |
| Ordered insertion | Existing actual input scenario uses a legacy flow page | Actual nested Row/Column and button-carousel insertion with visible neighbor order; preserve capacity and identities |
| Mixed spans/full/locked grid | Native `studioShiftCascadesAreBoundedAndAtomic`; QML resize/reflow/refusal | Actual pointer/remote movement replay across mixed spans, full grid and locked neighbors |
| Decorative overlap | Legacy component-grid overlap test exists | Studio movement honors configured overlap without moving decorative neighbors; pointer/remote/cancel/undo replay |
| Reduced motion | New target/actions use stationary outlines and immediate changes | Actual enabled-setting movement replay with unchanged information and serialized output |
| Standing visual gates | Prior page-grid checkpoint reviews both fixture journeys | Populated Pastel reference remains incomplete; never count save/reset alone as recreation |

The current `gridMode` branch treats nested Row/Column layouts as placed grids. Legacy `flowMove` exercises array order through `orderRelative`; it does not establish nested insertion behavior. This is a concrete implementation/coverage item for the next T20 batch, not an accepted exception.

## Final graphical checkpoint

The exact full-e staged executable passes Studio and both save/reapply/reset journeys. All **23** captures under `build/vm-oracle/t20-group-targets-20261023-full-e/visual/` were opened and reviewed:

- Studio attempt `20261023-093231-205`, `/tmp/isolde-components-acceptance-g9s_1gu2`: 15 images. Move controls are readable and leave the candidate visible during reflow and long-page movement. Existing verbose Inspector and duplicate preview-banner text remain quality gaps under their owning tasks; this synthetic windowed journey does not qualify TV usability.
- Lounge attempt `20261023-093422-501`, `/tmp/isolde-reference-config-b7v_ahjv`: four images. Reviewed against the retained reference; configured landscape composition and reset pass with the documented illustrative artwork.
- Pastel attempt `20261023-093518-079`, `/tmp/isolde-reference-config-tusatcif`: four images. Save/reapply/reset passes, but visual recreation remains **incomplete**: one fallback-initial app and unavailable history fail the required populated composition. Wallpaper/launcher/panel geometry remains intact. No new visual acceptance or task checkbox is claimed.

Normal profiles retain exact hashes: Household `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. The service is restored active/running, PID 1950685, zero automatic restarts. Installed release stays `fc815f2341cb375a0b809118`. No deployment, profile edit, guest cleanup or host maintenance occurred in this slice.
