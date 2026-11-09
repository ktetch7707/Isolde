# Games catalog and artwork recovery boundary — 2026-11-09

This slice closes a specific managed-artwork data-loss gap. It does not complete
T30.3 or establish installed-VM/crash-power-loss acceptance.

## Previous failure mode

Catalog restore creates a private SQLite `VACUUM INTO` recovery copy, while
artwork is stored separately as content-addressed PNG files. Replacing or
clearing artwork and removing a game previously garbage-collected a blob using
only references in the live catalog. A later metadata cleanup could therefore
delete artwork still referenced by a retained pre-restore database copy. A
crash between atomic blob publication and catalog commit could also leave
unreferenced artwork consuming the bounded store indefinitely.

## Implemented boundary

- Artwork deletion checks the live catalog and every retained
  `games.sqlite.pre-restore-*.sqlite` recovery copy. If any copy is unreadable,
  unsafe, or cannot be queried, cleanup fails closed and leaves the blob in
  place.
- Catalog open scans managed content-addressed artwork for crash orphans.
  Cleanup runs only after it has loaded all live and recovery-copy references;
  it removes only regular, in-directory files whose bytes match their
  64-character SHA-256 filename and which no catalog copy references.
- Snapshot enumeration and the candidate list are bounded. Excessive snapshot
  or artwork counts disable cleanup rather than risk deleting data. Symlinks,
  malformed files, and hash mismatches are preserved for inspection.
- The same reference check is used after a successful artwork replacement,
  clear, or catalog-only game removal. JSON backups remain self-contained and
  continue to embed validated artwork.

## Regression coverage and limits

`catalogArtworkSurvivesRecoveryAndReclaimsCrashOrphans` creates a pre-restore
snapshot that references managed artwork, confirms later catalog changes do
not delete it, then simulates an interrupted blob publication and verifies
that reopening the catalog reclaims only the verified orphan.

The Qt regression is added to `games_remote_ui`, but Qt/CMake are unavailable
in this worktree, so it has not been compiled or run here. The test is not a
power-cut simulation. Filesystem durability across sudden power loss, recovery
copy retention/space policy, and full installed-VM catalog recovery remain
open.
