# T20 nested editing execution record

This is implementation evidence, not user usability acceptance. T24 still owns editor cutover and the uncoached user review. No physical HDMI switching or audio qualification is claimed.

## Implemented data and editing paths

- Components remain a bounded flat array with validated `parentId` links. Containers have a declarative internal grid, row or column layout. Parents must exist, cycles are refused, nesting is limited to four container levels, and grouping is limited to 32 selected identities.
- Placement, collision solving, section transfer and reparenting use the same proposal and complete-result validation path. Each parent has a separate collision space. Ancestor locks apply to child movement. Failed operations retain the complete draft and history.
- Group, ungroup, subtree duplication and subtree removal are atomic undoable commands. Grouping preserves cell positions and refuses an unselected component inside the proposed bounds. Ungrouping refuses an incompatible internal grid instead of rounding or resizing children silently.
- Shared rendering draws nested containers and their actual children. Page/section rendering excludes child nodes from its root grid. Runtime Confirm enters a group; Back restores its containing item. Studio maintains a separate parent scope and breadcrumb.
- Layers supports expansion, selection for grouping, reading-order inspection, explicit before/after ordering and reparenting. Remote movement, pointer movement, numeric geometry and pointer resize use the shared transaction path. Camera view controls do not write layout data.
- The packaged eight-input example is eight editable slots arranged in a 4×2 child grid. It displays capability unavailability; it is not evidence of connected devices or functional capture hardware.

## Frozen source and test history

All snapshots package tracked working-tree bytes from `5df7ae12c73d0217e185267ee7781d5bed78e026` using `tools/package-vm-source.ps1`. Archives and extracted per-file manifests are verified before CMake. Guest sources are under `/home/isolde-dev/isolde-runs/studio-implementation-N/source`; sibling build and log directories identify the exact tested tree. Extraction uses `tar xzmf` to avoid host/guest timestamp skew.

- Snapshot 38, archive `a42d538f5fc9c9b06354505de1d927b6610fc41121df2d38e0564f1484d8cf81`: initial hierarchy/prototype/grouping foundation; component suite 27 passed.
- Snapshot 39, archive `c12b401a35a85fea56f6ce3b3fdc75b7a7c51bc9ea8be005073ede12ac1353c8`: test compilation failed on an incorrect `dirty()` accessor. Corrected to `isDirty()`; not installed.
- Snapshot 40, archive `4384311897b693546c466b10c516025e264bd69eeff160d75d5cc1faaaf7d1fc`: 27 component cases passed; UI suite found two regressions. The geometry test needed the new parent argument, and selection read a stale dependent binding when entering a group. Both corrected; not installed.
- Snapshot 41, archive `066779706a78f503d0fe99ac986c0c208102c74b13955c4cbff0fccbbf0ffafe`: 27 component cases and 25 UI cases passed, including page and nested-group remote/pointer/numeric byte parity, actual child rendering and parent-scope cancel/recovery.
- Snapshot 42, archive `62c841690bb42bc7dd710d2edf5bed26133989be4af2757584c46485a0c0e08c`: 27 component cases passed; UI suite exposed an older test's assumption that the hidden component was the final Layers row. The new navigation-order command follows component rows. The test now finds the actual hidden row; not installed.
- Snapshot 43, archive `80e110b173230303e5809f017d22f7dad8fdf25238b6b3995cb860521fda0d99`: 27 component cases and 25 UI cases passed. Review then found that the outer shell intercepted Back before a runtime group could handle it. Full application compilation was deliberately stopped by signalling only Ninja whose verified working directory was this isolated snapshot's build directory. Normal service unaffected; not installed.
- Snapshot 44, archive `3c87c859f787cd1e194edf5259e712d0c7020535188b1fc672b551323ecd0253`: adds group-aware Back routing for Home/named pages and an actual named-page surface regression. Native UI suite passed; superseded before deployment.
- Snapshot 45, archive `58ff2e7d947cf38c2232c3fcc9ee33f79cf55edc3907c6dbdb10fb5f9da0af60`: adds parent-ID remapping when copying a complete nested page. Full application build and 14/14 CTest passed (66.20 seconds); not installed.
- Snapshot 46, archive `fa282aad3d865099acf484151e89d8dcfc72951f0db53c2c0bf26dff5bfb26c2`: adds visible internal-grid spacing errors, the corresponding Apply guard and camera-only no-write regression. First transfer was prevented by an automatic approval review usage-limit failure; resumed after the user's continue instruction. Test compilation then found a malformed initializer; not installed.
- Snapshot 47, archive `832c6a1953618369be16980a39bd5386250283ddc3cb86f707b208a04eb5f012`: 27 component cases, 25 UI cases and 14/14 full CTest passed (62.39 seconds). Installed as `cafae4b2194b3527c50a7ee2`. All eight Studio captures reviewed, but child selection text overlapped the group heading, and the expanded installed test sent an extra Back before Apply. This is a failed graphical checkpoint, retained at `build/studio-installed-47/` and `/tmp/isolde-components-acceptance-uwd_ry1a`, not task completion.
- Snapshot 48, archive `3e349c473149ae6ad53e056fdb895296755e68ea6be43e73925042fbdae238cd`: moves nested selection identification into the footer, adds an explicit no-overlap regression, and corrects the installed test's page-menu return sequence. Native component and UI suites passed; full application build and 14/14 CTest passed (64.50 seconds). Installed as `a4120c77640b16fbae8b0756`.

## Reproduction

Follow `docs/vm-oracle.md`. Build the component and navigation targets first, run both suites with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`, then build the complete application and run `ctest --test-dir ../build --output-on-failure`. Install only a passing frozen source using `tools/install-appliance.py install --build-dir ../build`; restart the user service and verify the release link and `/proc/<MainPID>/exe`.

Run `tools/run-visual-acceptance.ps1` sequentially for `studio`, `lounge` and `pastel`, supplying the exact frozen `-SourceRoot` and a new host evidence directory. The Studio journey captures editing, collision, resize, Try interaction, applied reopening, the input-slot group, child scope and expanded Layers. It checks pre-Apply bytes, atomic Apply, the v1 backup and all eight local placements. Capture acknowledgement alone is not visual acceptance: inspect every screenshot, copy the exact guest evidence directories, then restore and inspect normal Household Home.

## Final installed evidence

Expanded Studio journey `/tmp/isolde-components-acceptance-c1qahjv0` passed. All eight captures were explicitly reviewed: EDIT, COLLISION, RESIZE, STUDIO, APPLIED, GROUP, INSIDE and LAYERS. The child label no longer overlaps the group heading; its exact identity appears in the footer. The group contains eight distinct local placements in four columns and two rows, and all eleven components survive atomic Apply. Capability unavailability is visible, with no physical input claim.

Dark reference `/tmp/isolde-reference-config-obl_i1qy` and pastel `/tmp/isolde-reference-config-m59s6p9b` passed recreation, personal-skin round trip and packaged-default reset. All four fixture captures were explicitly reviewed. The fixtures still use the same generic renderer, without fixture-specific application branches.

Durable host evidence is `build/studio-installed-48/`: screenshots, exact copied guest evidence directories, native logs, full CTest and installation logs. `normal-service.log` verifies the restored Household service active/running with zero restarts and `/proc/<MainPID>/exe` pointing to the installed release. `normal-home.png` was reviewed. The existing intrusive Detected Sources rows remain visible and belong to T25; no normal profile or Kodi data was removed to conceal them.

## Remaining product acceptance

T20 is complete on the recorded native and installed evidence. The complete six-family catalog and contextual appearance inspector remain T21; reusable templates/tokens/reference overlays remain T22; Preview/Apply hardening and final cutover remain T23–T24. The duplicate Try-interaction banner wording is retained for T23 polish. Full-product offline Generator, genuine user review and physical input qualification remain open in their owning tasks.
