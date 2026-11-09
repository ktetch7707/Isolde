# Games catalog shelves coding slice — 2026-11-09

This change extends the profile-owned Games catalog and its shared library widget; it does not close T30.

## Delivered

- Added a bounded, case-insensitive platform category list and validated platform-filtered catalog queries. The Games widget can filter interactively with the remote, and configured library components can target a named platform.
- Added Favorites, Continue Playing and platform as validated Games shelf views. Continue Playing is populated only after a game session returns through `SessionManager`; failed launches do not create activity. The shelf stores only the game ID and last successful return time.
- Added an explicit, transactional catalog schema v3 write on the first successful return. Existing v1/v2 catalogs are not migrated at startup; older catalogs continue to work until a completed session creates the new activity table. Removing a game cascades only its profile-local activity record.
- Limited distinct platform categories to 128; imports remain atomic and existing metadata/source entries are preserved on refusal.
- Added native/offscreen regression coverage for case-insensitive filters, Favorites, empty and completed Continue views, and remote platform selection.

## Not delivered / verification boundary

This slice does not implement the rest of T30: enriched metadata (franchises/tags/artwork), saved/manual or declarative collections, full search over aliases/tags, catalog export/restore, Steam external-library connection, local installers, typed optical-disc handling, complete emulator/platform lifecycle guarantees, or the 5,000-entry installed VM matrix. It does not claim gameplay or hardware acceptance.

The environment has no CMake/CTest or Qt build tools, so the new C++/QML regression target could not be built or run here. Verify this code on the owning build/VM before treating its focused regression as passing.
