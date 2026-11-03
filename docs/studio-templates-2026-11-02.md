# T22 starter-template packaging — 2026-11-02

## Implemented and focused verified

Four data-only editable starters are packaged through the existing Saved designs / skin picker, after the three existing built-ins: Compact launcher, Scrolling dashboard, Poster library (left page navigation), and Minimal pastel. `config/skins/templates.json` owns catalog names and descriptions; each layout lives in its own JSON document. Existing schema, renderer, app registry and Kodi sources remain authoritative. No new provider, executable template or parallel persistence format is introduced.

Production `HomeLayout::applySkin` validates each template and applies it through the existing atomic skin pipeline. These templates contain no page collection, embedded artwork, credentials or machine paths. Existing authored pages are preserved. Catalog IDs must be safe, unique, bounded and distinct from existing reserved built-ins. The first three built-in positions remain unchanged; personal skins follow the starters.

`t22-starter-templates-20261102-d` passes `component_contracts` **1/1**, zero failed/skipped CTests. `starterTemplatesAreEditableAndPreservePages` exercises all four layouts through production apply, edits a component name, reloads from disk, checks exact existing-page preservation and refuses an unsafe ID without document mutation. Existing skin atomic-write rollback, snapshot, artwork and profile regressions pass in the same suite. Source digest `d5d2a8e0ed14674e77a54d51cf02504865fde195c31a61ff075ae8249a5c591a`.

Evidence: `build/vm-oracle/t22-starter-templates-20261102-d/report/ctest.log`, `report/ctest.xml`, `report/status.json` and `snapshot/files.sha256`. Service remains active with zero restarts; installed release remains `1302234b59324d602e48e9d9`. Tests used a separate index at `build/t22-template-test.index` so pre-existing untracked dependencies could be included without changing the real staging index.

Rejected runs remain diagnostic: A was blocked by sandbox transport/index permissions; B caught the new test calling a private method (corrected to `committedDocument`); C caught invalid section-local grid modes in the dashboard. D corrects the dashboard to a screen grid with explicit vertical scrolling and non-overlapping placements.

`git diff --check` passes for the touched source/resource/test files. `clang-format` is unavailable on this host; formatting conformance is not claimed. No host tool was installed. No full application suite, staging/deploy, installed visual review or reference-pair acceptance was run for this narrow data/catalog milestone. The CMake resource additions still require the task's eventual application build/runtime verification.

## Remaining T22 work

T22.1.b/.c are focused verified; T22.1.g retains installed template/fixture acceptance. Next is T22.1.d: scope-aware draft preview/application. Scope-aware draft preview/application, responsive section rules, private design-reference imagery, shared tokens/reusable symbols and secure import/export remain open. No original parent acceptance box is closed.

## Named roles focused verification

T22.1.c passes in `t22-template-roles-20261102-b`: `component_contracts` 1/1, source digest `215cb06e06ea39038e6d19343ac1153f625600308a3a0544d0746c2903c3c98f`. The additional regression edits named app and Kodi content roles through Studio with no Kodi client, selects an empty app collection, and verifies cancel restores exact draft bytes while applied and disk bytes stay unchanged. The first role attempt used an incomplete presentation map; the corrected regression retains the existing presentation as the inspector does. No provider availability is inferred from editability. Next: T22.1.d scope-aware draft preview/application.

Home/picker regression `t22-template-picker-20261102-a` passes `home_component_navigation` **2/2** (Home component navigation and Studio navigation), source digest `f60e1a6b611269fb8ef6468b64eb64ec1edd134357a0ee9e2cf7ca75fa941caf`. This covers expanded picker counts and existing remote skin operations; it is offscreen evidence, not installed visual acceptance.

Final catalog-boundary run `t22-template-catalog-20261102-a` passes `component_contracts` **1/1**, including both new template regressions and fractional-version refusal with no mutation. Source digest `08ede527696e8bd70ea376d222cc8c94d45ed657e68c2dce9954f540305f2017`. The only application change after the picker pass tightens catalog metadata acceptance; valid packaged entries and picker behavior are unchanged. Reuse the picker pass for that boundary. The installed service remains active with zero restarts and its release is unchanged.
