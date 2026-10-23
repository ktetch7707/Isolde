# T20 cross-section group movement — 2026-10-23

Cross-section group transfer now preserves the view in which the gesture began. Back restores All sections versus isolated section, the original section/group/component and captured viewport, without changing document/history. Previously choosing a destination switched to isolated view and cancellation failed to restore All sections.

Destination choices and the active preview identify the containing section and parent-group path using authored names. This distinguishes groups across sections without deriving destinations from incidental pointer overlap. Placement continues through the existing validated shared proposal and single transaction.

## Evidence

- `t20-section-targets-20261023-a` fails fixture setup: explicit grid conversion packs the third cell into the intended destination. The diagnostic capture was opened; the fixture removes that unrelated third cell before constructing the two groups.
- Run b reaches the real transfer and fails exact cancellation: destination view leaves All sections disabled and removes the destination group from the restored viewport. The capture was opened. This is the application regression fixed here, not a waived assertion.
- Run c passes `studio_destinations` in 13.86 seconds, including actual remote/menu-started pointer selection, nonzero target cell, one accepted history entry, canonical output parity, exact undo and no disk writes. Both captured group previews were opened. This targeted run precedes the final two-row extension below.
- Final coverage replays the transfer from both All sections and isolated-section views. It checks the destination section/group, visible candidate, contextual label, unchanged preview document/history, original view and exact viewport on Back, accepted parent identity, remote/pointer document equality and exact Undo. Full-d passes 25/25 native registrations in 276.05 seconds; its destinations registration passes in 16.91 seconds. Source digest `4bcb2d9181331427333a7740053443be24e347ea91fee710b7c7390a8d70d56c`; source archive SHA256 `f2f4f585705b15394d35cad82ab83809b98c56a8b78128201ebdc537d8d863df`. Both new cross-section captures were opened; fifteen other offscreen captures match the reviewed policy full-b checkpoint byte-for-byte. Graphical review is recorded below.

These sparse fixtures establish edit behavior and visibility, not professional default-design acceptance. T20 stays 80%; Overall round(866/24) = 36% across 24 active tasks. Installed acceptance and populated Pastel recreation remain open. No deployment or normal-profile modification is part of this slice.
## Final graphical review

The exact full-d staged executable passes all three functional journeys; all 23 graphical captures under `build/vm-oracle/t20-section-targets-20261023-full-d/visual/` were opened:

- Studio attempt `20261023-104905-036`, `/tmp/isolde-components-acceptance-5e1_yh44`: fifteen captures. Movement candidates and actions remain visible; nested edit/apply and scrolling journeys pass. Verbose Inspector notices and duplicate preview text remain unaccepted polish work.
- Lounge attempt `20261023-105054-741`, `/tmp/isolde-reference-config-9l_glubx`: four captures. Populated hero/history/source/guide composition retained; save/reapply/reset passes. Its reset default remains visually basic.
- Pastel attempt `20261023-105149-036`, `/tmp/isolde-reference-config-n71ann_e`: four captures. Header/source/background geometry retained and save/reapply/reset passes, but one initial-based app and unavailable history leave populated reference recreation **incomplete**. Functional green is not visual acceptance.

Final report is passed/finished, normal service active/running PID 1976871 with zero automatic restarts. Both normal profile hashes are unchanged: Household `87aedaa1a40f7857ac54d06398aade1e2b41e18182aad698b248058c21d6fa6e`; Studio Demo `895804ffc1eb2167f5b723047116564742f70701a6d07eaf705b9928c59efdac`. Installed release remains `fc815f2341cb375a0b809118`. No deployment, guest cleanup or host maintenance occurred.