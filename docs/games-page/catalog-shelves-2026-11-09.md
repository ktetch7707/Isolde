# Games catalog shelves coding slice — 2026-11-09

This change extends the profile-owned Games catalog and its shared library widget; it does not close T30.

## Delivered

- Added a bounded, case-insensitive platform category list and validated platform-filtered catalog queries. The Games widget can filter interactively with the remote, and configured library components can target a named platform.
- Added Favorites, Continue Playing and platform as validated Games shelf views. Continue Playing is populated only after a game session returns through `SessionManager`; failed launches do not create activity. The shelf stores only the game ID and last successful return time.
- Added an explicit, transactional catalog schema v3 write on the first successful return. Existing v1/v2 catalogs are not migrated at startup; older catalogs continue to work until a completed session creates the new activity table. Removing a game cascades only its profile-local activity record.
- Limited distinct platform categories to 128; imports remain atomic and existing metadata/source entries are preserved on refusal.
- Added profile-local franchise/tags with bounded validation, remote editing and local search across title/franchise/tags.
- Added v4 organization tables, manual collections with ordered membership and declarative smart collections matching platform, Favorites, or title text. The remote can create, browse, remove collections and add/remove game membership. Removing catalog records cascades only their organization/activity rows.
- Added profile-local JSON catalog export and explicit-confirmation restore. Archives are byte/record bounded and validated before replacement; imports retain media-folder grants, verify identity/collection references, preserve a private SQLite recovery copy, and transactionally replace only catalog data.
- Added native/offscreen regression coverage for case-insensitive filters, Favorites, Continue views, metadata validation/search, smart and manual collection behavior, archive rejection/restore and remote platform selection.

## Not delivered / verification boundary

The Games catalog still lacks aliases and rich summaries, managed cover-art import, mutable smart-rule editing and collection reordering, Steam external-library connection, local installers, typed optical-disc handling, complete emulator/platform lifecycle guarantees, and the 5,000-entry installed VM matrix. It does not claim gameplay or hardware acceptance.

The environment has no CMake/CTest, Qt, or C++ compiler, so the new C++/QML regression target could not be built or run here. Verify this code on the owning build before treating its focused regression as passing. This is deferred verification, not a coding blocker.
