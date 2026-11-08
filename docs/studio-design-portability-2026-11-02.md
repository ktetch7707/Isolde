# T22 design drafts, portability and private references — 2026-11-02

## Implemented batch

Normal Studio **Saved designs** now opens a design workflow inside the existing session, including dirty drafts. Starting points and saved copies enter the same proposal model as other previews. **Current page** is the default and preserves the other pages and their identities; **Whole interface** explicitly lists page additions, replacements and removals. The preview defaults to Cancel. Accept is one undoable draft operation; only Review & Apply writes applied layout/artwork. Managing personal copies retains the legacy library, but choosing its Apply action returns to Studio scope/preview instead of saving directly. Back returns to the original page/component.

Portable export writes a new `.json` file; an existing file is refused. Current-page and whole-interface formats share a version-1 envelope (`schemaVersion`, `kind`, `layout`, `assets`) with version-2 declarative layouts. Import validates the entire supported document and proposes it without modifying the applied profile. Missing apps remain editable placeholders; the remote picker can remap them to registered apps by name. Missing page destinations are refused with a repair/scope message, never silently redirected. Import artwork is staged privately and becomes profile-owned only on successful atomic Apply.

The file browser supports bounded local folder traversal, parent/previous/next and a remote keyboard path alternative. Folders with more than 512 entries are refused; results are paged 64 at a time. Symlinked files/parents, nonregular files, unknown package versions/fields, unsafe assets, unknown executable fields, private paths and credential-bearing URLs are rejected. Packages are capped at 64 MiB; the existing layout (4 MiB / 1024 nodes / 64 pages), image and staging budgets remain in force. The browser performs no network request.

Editor reference images use a separate temporary store, not profile artwork. PNG/JPEG only, up to 8 MiB / 16 megapixels; opacity, show/hide and remove are remote operable. Invalid replacement preserves the previous image. Closing/invalidating the session removes the temporary image. Neither Apply nor portable export includes reference state or its bytes.

Supported portable content currently consists of the shared declarative component/appearance models, managed artwork, registered-app identities, page routes and generic Kodi/library roles. Machine-specific file/disc/media/game/collection bindings and credential-bearing paths must be replaced with portable roles before export; the current source is never rewritten to accomplish this. Remaining source-remapping and full supported-matrix requirements stay in final T22 acceptance rather than being relabeled external limits. Tokens, reusable linked sections and responsive section rules remain open.

## Verification plan and actual outcomes

The combined candidate owns new native `studio_design_contracts` and QML `studio_designs` suites. They exercise page/interface preview, unrelated-page preservation, cancel, one-step Undo, missing apps/remap, asset round trip, apply failure/retry, malformed/private data rejection, source-profile/personal-skin preservation, export no-overwrite, reference bounds/cleanup/exclusion and actual remote/pointer controls. QML scenarios save preview/reference captures for review.

`t22-design-portability-20261102-a` stopped at build before any application test: concurrent T23 test code called private `AppCatalog::installCustom`. The current source uses its public `addWebApp` denial boundary. New T22 C++ and test code compiled in A. Candidate B resumes the same combined full scope from the incremental cache. Candidate B compiled, and new native design contracts passed, but the full run failed two legacy timeout budgets, new QML regressions and a concurrent composition-fixture error, then exceeded its 600-second aggregate bound. It is not integration evidence.

The runner now supports `-Suite studio_designs`, selecting the six affected suites together (`studio_quick`, `studio_designs`, `studio_cutover`, `studio_colors`, `component_contracts`, `studio_design_contracts`), and the broader `studio` family includes them. The two host selection contracts pass. An attempted broader Windows harness invocation was blocked by sandbox temporary-directory access; the targeted checks pass with temporary files confined to the workspace. Linux runner self-tests remain mandatory in the VM. No host analyzer/toolchain was installed; clang-format/qmlformat/Ruff availability is not claimed.

## Remaining acceptance

Final installed/runtime/fixture acceptance, wider source mapping, responsive rules, tokens/reusable symbols and complete T22 closure remain open. A passing full suite is integration evidence, not automatic visual approval. This record will identify the exact candidate, source applicability and every reviewed capture before crediting tested milestones.

## November 3 diagnosed regression results

`t22-design-regressions-20261103-a` passes five of six suites: Studio quick 50.67s, designs 13.52s, colors 55.18s, component contracts 6.87s and native design contracts 0.22s. Cutover alone fails on delayed navigation callbacks dereferencing destroyed HomeScreen/ComposedHome views. Those callbacks now guard their owning object before accessing generation or viewport state. Run B checks the same six suites together; no replacement full run was started before resolving these failures.

Agent reviewed both A captures: `build/vm-oracle/t22-design-regressions-20261103-a/report/studio-test-design-preview.png` and `studio-test-private-reference.png`. Preview exposes changed Home, one draft transaction and focused Cancel; the private reference fills the editor, and opacity/hide/remove controls are readable. This is focused editor evidence, not final installed fixture acceptance.

`t22-design-regressions-20261103-b` confirms the lifetime failures are fixed and the 1,024-node layer test passes. The six-suite run still fails: a restore-defaults assertion predates the new Whole interface option, and a long wrapped label reports a dialog-height binding loop. The shared assertion now checks both explicit scopes. Label height no longer anchors back to the height it determines. Run C checks these changes and the new navigation-metadata summary regression together. No suite timeout was raised.

Navigation-only design differences now explicitly list Home metadata and startup-page changes even when the component payloads are identical. The native regression exports a design, changes only those two fields, previews two summary entries, then cancels back to exact draft bytes.

## Verified combined checkpoint — 2026-11-03

`t22-design-regressions-20261103-c` passes **6/6**: Studio quick, remote designs, cutover, colors, native component contracts and native design contracts. Source digest `df25dd4bfec46515a9f9e3d756c75e2961723314a5f7d05616fd2d6936552239`; exact source archive SHA256 `d205daa9617c21520eba62e2e36f812491bec0212a8635a47ed2685f4cfbb70b`. Evidence: `build/vm-oracle/t22-design-regressions-20261103-c/report/ctest.log`, `ctest.xml`, `state.json` and archived snapshot. This includes the metadata-only summary regression and all named failure corrections. Service remains active/running, zero restarts, installed release `1302234b59324d602e48e9d9` unchanged.

Credited narrow milestones T22.1.d/.f, T22.3.a/.b and T22.4.a; no parent acceptance, installed gate, T23/T24 closure or full-suite pass is inferred. Remaining responsive layouts, tokens/reusable symbols and final installed/supported-matrix acceptance stay open. Reuse this focused evidence; a further full run belongs to task closure or a separately recorded material boundary change, not this ledger update. Shared T23 implementation is still uncommitted, and this snapshot intentionally includes that dependency; do not represent it as a standalone HEAD build.

The C preview/reference captures are byte-identical to the two agent-reviewed A captures: SHA256 `12c0c139b215f6d7e48db15c2fd4c78a69ad92bcbb4c20f9e5713a96e538aa01` and `3333dba52e502b9fdc16986329981639f4baa49f28694ef0c145a5be86c6f322`. Review is therefore applicable to the passing candidate. Progress accounting passes `tools/backlog-progress.ps1 -CheckTable`: T22 **47%**, sum **1183**, Overall **44%** across the fixed 27 active tasks.
