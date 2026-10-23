# Fast Studio tests — no deployment

Use the existing isolated Linux VM and cached Ninja build. No Windows Qt installation, host application execution, new shared folders or system configuration is needed. `Test` transfers the tracked project snapshot, builds selected test targets and runs offscreen. It does not select an installed release, restart Isolde, launch Kodi or run graphical acceptance.

## Commands for coders

Run from the project root in PowerShell 7:

```powershell
# Small QML controls/rendering suite while editing the UI.
./tools/vm-test.ps1 Test -Suite studio_quick

# Data/transaction rules while editing schema, reflow, history or save logic.
./tools/vm-test.ps1 Test -Suite studio_unit

# Real editor workflows, including scrolling and cancellation.
./tools/vm-test.ps1 Test -Suite studio_behavior

# All three focused suites for a completed Studio change.
./tools/vm-test.ps1 Test -Suite studio

# Full native regression gate at an integration/release checkpoint.
./tools/vm-test.ps1 Test
```

Only the last command stages a deployment-capable payload. **None deploys.** Use the printed run ID for `Status`; poll an existing running tool session instead of starting another build. A targeted pass cannot be used for Visual or Deploy. Read [vm-oracle.md](vm-oracle.md) for the periodic graphical/release workflow, safety boundaries and failure decisions. No automatic deployment or retry loop is introduced.

During development, run the relevant small suite; run `studio` after a complete feature. Before release, run the full native suite and applicable graphical fixtures, inspect captures, then explicitly Deploy the accepted run. Test infrastructure changes with unchanged application code reuse the previous accepted application visual evidence; they still require native verification. Hardware/disconnected cold-boot gates remain separate. Test count and duration are measured in the evidence record, not promised deadlines.

## What runs

| Suite | Implementation | Checks |
| --- | --- | --- |
| `studio_quick` | Qt Quick Test; `tests/studio/tst_editor.qml` | Thirteen real QML scenarios below, remote commands and actual pointer input |
| `studio_selection` | Qt Quick Test; `tests/studio/tst_selection.qml` | Three scenarios for authored selection geometry/persistence, transparent recovery and pointer/remote switch parity; newly added, execution pending VM capacity |
| `studio_unit` | Existing Qt Test component executable, seven selected functions | Draft/history bounds, atomic failure/rollback, profile scope, collision parity, bounded cascades, scrolling persistence and home protection |
| `studio_behavior` | Existing Qt Test navigation executable, four selected functions | Remote draft/apply/cancel, gestures/collisions/anchors, section transfer/cancel and long-page Add/move/cancel/runtime visibility |
| `studio_gestures` | Qt Quick Test; `tests/studio/tst_gestures.qml` | Actual pointer threshold/cancel/release, ordered insertion, bounded edge growth, remote canonical parity, resize reflow/refusal, visual alignment and group-layout transactions |
| `studio` | Five suites above | Builds only their three test executables; excludes unrelated navigation and Kodi journeys |

The complete existing suites remain registered. These additional focused registrations deliberately reuse tested functions instead of copying their code. Full CTest includes both focused and broad registrations; a focused pass is not a substitute for full integration coverage.

## Executable behavior scenarios

These are Qt tests with readable Given/When/Then contracts, **not a Cucumber installation or a custom Gherkin parser**. Each row maps directly to an executable `test_...` function in `tst_editor.qml`.

| Scenario/function suffix | Given | When | Then |
| --- | --- | --- | --- |
| `move_cancel_restores_document_history_and_viewport` | A scrolling page | Move twelve rows down, then Back | Exact draft/history/disk and scroll position restored |
| `size_slider_remote_cancel_and_pointer_commit` | A one-cell component | Resize with remote, cancel; resize using the real Slider pointer control and accept | Remote preview writes nothing; pointer accepts one edit, Undo restores exact document |
| `no_room_enable_and_add_is_one_undoable_edit` | A completely occupied fixed page | Add a configured Text component and choose scrolling recovery | Configuration retained; policy and component accepted together; one Undo restores both |
| `scrolling_checkbox_pointer_preview_cancel` | A fixed page | Click the real checkbox, then Back | Preview changes without changing draft/history/disk; Cancel removes proposal |
| `move_reveals_actual_rendered_component` | A scrolling page | Move twelve rows down | Actual proposal item is wholly inside the viewport; Confirm accepts once without saving |
| `locked_component_cannot_start_moving` | A locked component | Attempt Move | Clear refusal, unchanged draft/history/disk |
| `page_color_swatches_preview_cancel_and_accept_once` | Page appearance | Choose swatches, cancel, then accept a changed color | Live model changes without a draft write; one accepted edit and exact Undo |
| `color_wheel_pointer_remote_parity_and_cancel` | A custom wheel | Select red/cyan by actual pointer input and equivalent remote input | Expected distinct colors, identical authored document, pending changes cancel safely |
| `page_color_reset_removes_override_and_is_undoable` | A custom text color | Preview Reset to theme and accept | Removes the override once; Undo restores it |
| `exact_color_validation_keeps_preview_private` | Optional exact input | Submit invalid then valid RGB | Invalid text refused; valid color previews without saving; Cancel restores |
| `scalar_remote_cancel_pointer_commit_and_parity` | Opacity slider | Reach endpoints by remote/pointer, cancel or accept | Percentage display, exact canonical parity, one edit and no disk writes |
| `scalar_exact_percent_preview_and_reset` | Page dimming | Open optional exact input during pending preview, reject invalid values, cancel/reset | Percentage conversion, private transaction, exact cancellation and undo |
| `scalar_corner_presets_and_custom_value_preservation` | An existing custom corner value | Open slider, choose named presets by remote and accept | No snap on open, context fits viewport, one accepted edit and exact Undo |

## Adding tests

- Test the production QML and validated session APIs. Mock external providers, not placement, rendering or transaction behavior. No real Kodi/network client is supplied by the Quick Test fixture.
- Give every scenario a new saved document in `QTemporaryDir`; close/invalidate its Studio session during cleanup. Never read normal Household/Kodi data.
- Use `tryCompare`, `tryVerify` or signal observation to wait for a condition. Do not grow arbitrary sleeps or global timeouts to make a failure disappear.
- Check real rendered items for clipping/visibility, not just calculated rectangles. Both device input paths must reach the same draft transaction.
- Wrap new scenarios in `scenario(name, body)`. Assertion failures save `studio-test-<name>.png` to the run's evidence folder. Only that allowlisted filename form is bundled; failures still fail the run. Captures are diagnostics, not automatic visual approval.
- Compile/import failures appear in `ctest.log`; they cannot produce an editor screenshot. Binding loops, ReferenceError and TypeError warnings fail the QML scenarios.
- The fixture reports no provider integration or aesthetic acceptance. Both standing visual fixture pairs and human review remain necessary at graphical checkpoints.

Logs, aggregate JUnit and failure screenshots are fetched to `build/vm-oracle/<run-id>/report/`. The Qt Quick test output names each scenario. A build target/suite selection mismatch, skipped test, failure or empty report cannot count as a pass.

Test records include before/after installed-service and release identities. An unexpected change fails the test run; the runner does not try to repair it by deploying. Failure-image bundling is covered by the runner's allowlist regression.

Framework references: [Qt Quick Test](https://doc.qt.io/qt-6/qtquicktest-index.html), [QML TestCase and input/wait/image helpers](https://doc.qt.io/qt-6/qml-qttest-testcase.html).
