# T21.6 collection reconciliation — 2026-10-30

S03 remains selected. Membership and editing milestones are accepted at native/QML scope; installed collection launch/return remains open. No broad T21.6 checkbox is closed.

## Evidence and applicability

`build/t21-collections-reconciliation.json` compares raw file SHA-256 values with full `studio-pastel-see-all-20261030-g` (41/41). ComponentSchema, StudioSession, PageAppCarousel, StudioAppPicker, StudioInspector, native component tests and ordered QML tests match exactly. Full G passed `studioButtonCollectionsAreBoundedAndAtomic`, button-carousel ordered insertion, locked/mixed/overflow cancellation, scoped button gallery, All/Selected membership and Android collection cases. The current batch changes only two data-defined display names and selection regression coverage, alongside this documentation. Compatibility ID `app-bar` stays intact; no saved layout migration occurs.

Focused `studio-collections-20261030-a` passes `studio_selection` in 38.72 seconds, source digest `1460a5b71fab5151ec8ffeb08de2c2989c569ed7d38c9645f9072e634cb84c72`. The runner reports passed, fullSuite false; normal installed service PID 2124904 remains active with zero restarts. Installed executable remains J, not the renamed candidate.

## Accepted narrow outcomes

- **T21.6.a:** All/Selected uses eligible desktop/web/Android registry identities; authored app IDs retain order and missing identities remain repairable. Button collections contain ordinary stable button children, bounded at 128; the native test rejects 129 without changing the draft, rejects newly unknown app IDs, retains previously missing IDs, and verifies exact removal/Undo with no saved write. Containers own membership rather than generated page placements.
- **T21.6.b:** `test_visual_app_picker_order_cancel_missing_and_pointer_parity` verifies real remote/pointer reorder output, Back cancellation, selected missing members and refusal of Select all beyond 128. Existing overflow/capacity/Undo cases pass. The new `test_open_gallery_registry_changes_preserve_scope_and_recover_focus` changes the live registry while More is open: install/reorder retains selected identity; removal leaves a reachable gallery selection; Back returns to More; selected order excludes unrelated entries; missing app refuses launch; reinstall recovers the original ID and restored gallery focus. Canonical membership and fixture disk remain unchanged. This is QML registry-boundary evidence, not an actual package install or native client lifecycle claim.

The editor now displays **App carousel** through packaged catalog/prototype data. The optional, explicitly configured App Store shortcut remains a separate existing gallery choice; it does not replace eligible collection membership.

## Remaining S03 acceptance

T21.6.c needs a bounded installed journey exercising collection population changes, overflow and a real ActionDispatcher/SessionManager client launch/return with exact gallery/card/page origin, plus capture review. Existing Pastel fixture asserts More presentation/membership and Back, but explicitly never launches its installed-state fixture apps. SignalSpy launch tests and synthetic installed-state fixtures must not be relabelled native lifecycle evidence. Reuse installed J for unchanged renderer/session bytes; the data-only catalog rename requires its own applicable visual check before broad acceptance. Keep source/normal-profile hashes and guest service restoration evidence. No full suite was run solely for naming, tests or accounting.
