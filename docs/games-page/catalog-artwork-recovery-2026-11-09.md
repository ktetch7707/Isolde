# Games catalog and artwork recovery boundary — 2026-11-09

This slice addresses managed-artwork data loss conservatively. It does not
complete T30.3 or establish installed-VM/crash-power-loss acceptance.

## Concurrency hazard and safe behavior

Catalog backups use SQLite snapshots while artwork blobs are published
separately. A process can publish or reuse a content-addressed blob before its
catalog transaction commits. Another process cannot safely conclude that an
apparently unreferenced blob is disposable: it may be in the middle of a
publication, a restore may be creating a snapshot, or a retained snapshot may
use a different schema/version. Read-only inspection of current snapshots
cannot establish exclusive ownership or close these races.

Therefore Games does **not** automatically delete managed artwork blobs:

- Replacing or clearing artwork changes catalog references only; prior blobs
  are retained.
- Removing a game remains catalog-only for files and artwork alike.
- Opening the catalog performs no artwork garbage collection. Crashed
  publication or restore may leave unused blobs; they are retained rather
  than silently removed.
- This preserves artwork referenced by any retained snapshot, including
  `games.sqlite.v1-backup.sqlite`, `games.sqlite.pre-restore-*.sqlite`, and
  other compatible or future snapshot files, without assuming that the list
  or schema is complete.
- Store exhaustion is reported by the existing bounded artwork importer. A
  future reclamation feature requires a cross-process coordination protocol
  covering publication, catalog commit, snapshot creation/retention, and
  deletion; no such lock is implemented here.

JSON backup export remains self-contained and continues to embed validated
artwork.

## Regression coverage and limits

`catalogArtworkSurvivesRecoverySnapshotsWithoutGarbageCollection` verifies that
artwork is retained after replace/clear, that artwork referenced by the legacy
`.v1-backup.sqlite` snapshot survives, and that reopening does not silently
delete an orphan left by interrupted publication.

The Qt regression is added to `games_remote_ui`, but Qt/CMake are unavailable
in this worktree, so it has not been compiled or run here. The test does not
simulate an inter-process race or sudden power loss. Broader catalog write
failures, durable recovery, and installed-VM acceptance remain open.
