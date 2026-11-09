# Games catalog shelves coding slice — 2026-11-09

This change extends the profile-owned Games catalog and its shared library widget; it does not close T30.

## Delivered

- Added a bounded, case-insensitive platform category list and validated platform-filtered catalog queries. The Games widget can filter interactively with the remote, and configured library components can target a named platform.
- Added Favorites, Continue Playing and platform as validated Games shelf views. Continue Playing is populated only after a game session returns through `SessionManager`; failed launches do not create activity. The shelf stores only the game ID and last successful return time.
- Added an explicit, transactional catalog schema v3 write on the first successful return. Existing v1/v2 catalogs are not migrated at startup; older catalogs continue to work until a completed session creates the new activity table. Removing a game cascades only its profile-local activity record.
- Limited distinct platform categories to 128; imports remain atomic and existing metadata/source entries are preserved on refusal.
- Added profile-local franchise/tags with bounded validation, remote editing and local search across title/franchise/tags.
- Added v4 organization tables, manual collections with ordered membership and declarative smart collections matching platform, Favorites, or title text. The remote can create, browse, rename/edit smart rules, remove collections, add/remove game membership and reorder manual entries. Collection counts reflect current rule matches. Removing catalog records cascades only their organization/activity rows.
- Added v5 aliases and bounded plain-text summaries with remote editing and local search across aliases/summary. Existing catalogs upgrade to v5 only on an explicit metadata edit; startup remains read-only for v1–v4.
- Added profile-local managed cover artwork (schema v6): remote import reads only a profile-approved PNG/JPEG, validates image size/dimensions, normalizes it to private content-addressed PNG, and renders it on the shared game cards. Clearing/replacing artwork never edits the source file.
- Added profile-local JSON catalog export v3 with embedded artwork and explicit-confirmation restore (v1/v2 archives remain supported). Archives are byte/record bounded and validated before replacement; imports retain media-folder grants, verify identity/collection references, preserve a private SQLite recovery copy, and transactionally replace catalog references.
- Added profile-local approval and remote management for up to eight external Steam libraries under `/media` or `/mnt`. Approved folders mount read-only in isolated Steam sessions; discovery and direct launch use only those grants, and revocation leaves files/catalog entries untouched.
- Added a typed, bounded read-only observation list for mounted ISO9660/UDF PC optical volumes, surfaced through the remote Games tools. The observation hash is explicitly not a durable media-set identity; no autorun, install, play, or eject is implemented by this inspection-only slice.
- Added native/offscreen regression coverage for case-insensitive filters, Favorites, Continue views, metadata validation/search, smart and manual collection edits/order, archive rejection/restore and remote platform selection.

## Not delivered / verification boundary

The Games catalog now has a conservative external Steam library approval/mount path and an inspection-only optical volume observer. Local installers, typed optical-disc identity/install/play/eject workflows, complete emulator/platform lifecycle guarantees, and the 5,000-entry installed VM matrix remain open. These changes do not claim gameplay or hardware acceptance.

The environment has no CMake/CTest, Qt, or C++ compiler, so the new C++/QML regression target could not be built or run here. Verify this code on the owning build before treating its focused regression as passing. This is deferred verification, not a coding blocker.

For the Steam grant slice, the local Python Steam adapter tests pass 11/11 and Games provisioning passes 14/14. Native sandbox and remote-QML regressions were added but could not be built or run in this environment.
