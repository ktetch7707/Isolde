# T29 — Store updates, cancellation and Android setup

## User controls

- **Menu → Check app updates** checks installed F-Droid apps subscribed in the
  active profile. F-Droid access must be explicitly ON. The Store shows an
  **Update available** badge and an **Update to …** action. Review/confirm is
  required for each update; nothing installs automatically.
- An installed F-Droid app has **Unsubscribe from updates** / **Subscribe to
  updates**. Unsubscribing keeps the app and its data, removes its update offers,
  excludes it from subsequent checks, and suppresses late results. Preferences
  are saved atomically per profile, survive restart and remain remembered after
  removal/reinstallation. Re-subscribing permits the next explicit check.
- **Cancel operation**, Confirm or Back cancels an active download/Android Store
  operation. The UI shows cancellation/rollback progress. The final atomic
  commit briefly disables cancellation. Android setup cancellation waits for
  the current system step, preserving completed setup work for retry.
- **Menu → Set up / repair Android** opens a Cancel-first confirmation with a
  keep/default or software-renderer choice. It downloads/configures the existing
  pinned Vanilla Waydroid/SmartTube baseline and its safe Store support. It
  requires internet and administrator approval. No password is collected by
  Isolde; the desktop's native authentication agent handles approval.

Web apps continue to receive their site's changes from the provider. Their
content is not an Isolde-managed APK update. Curated bundled app payloads remain
owned by the trusted distribution provisioner; this new repository-update flow
applies to installed F-Droid packages.

## Persistence and Android transactions

`AppCatalog` stores bounded `updatesDisabled` identities and a `storeTransaction`
token in the existing profile activation document. The provider receives only
subscribed package identities for update checks and returns at most 256 signed,
compatible candidates; browsing remains paginated. Failed preference writes
restore the previous in-memory choice. OFF clears offers and cancels requests.

An update stages the verified newer APK beside the old version. APK signatures,
package identity/version and the installed APK hash are checked; the old package
name merely remaining in Android's app list is not considered update success.
Registry replacement keeps the same identity without a remove/re-add interval.
Activation changes only after the owned Android session has stopped and the
helper reports a prepared transaction. Atomic persistence of the random token
is the commit authority; a pipe acknowledgment only wakes the helper.

Protected Android data needs a root-owned backup helper. The in-Store setup
builds/installs `isolde-android-support` using the existing fixed dpkg/pkexec
pattern. It never elevates a user-writable Python script and adds no blanket
authorization policy. The trusted root entrypoint accepts only validated
profile/config paths owned by the actual `PKEXEC_UID`; it holds directory
descriptors without following symlinks. Store operations may request native
administrator approval for backup/recovery. The appliance dependency manifest
now explicitly includes its existing `pkexec` mechanism.
Setup holds the same session lock as Android launches/Store operations, so a
concurrent launch cannot race system configuration or interrupt an active app.

Before mutation, the helper snapshots the selected profile's Android data and
package receipt with ownership preserved, then fsyncs the snapshot and journal.
Root-private backup storage is separate from app-writable data. Backups are
bounded to **8 GiB / 100,000 entries**, require sufficient free space and the
same filesystem as the Android data for atomic rename recovery; an unsupported
or failed backup refuses mutation. It never snapshots another profile or stops
a foreign Android session.

Cancellation or failed activation restores the previous data/receipt. A saved
commit keeps the new data. Interrupted recovery resumes from its journal;
durable outcome records distinguish completed cleanup from a missing backup.
Pending-recovery markers prevent ordinary launching before required recovery.
Denial/failure of protected cleanup is surfaced for retry, never reported as a
completed rollback. Android app-data rollback is not an attempted downgrade
that uninstalls the user's app/accounts.

## Verification and handoff

No VM actions, real system setup, Android installation, C++ build or rendered
GUI acceptance were run. The user delegates those checks to another agent.
Host tests use isolated workspace files and mock privileged commands.

- `test_fdroid_store.py`: **14 tests passed**, including subscribed-only update
  selection and rejection of invalid subscription requests before networking.
- `test_store_lifecycle.py`: **18 tests OK, 1 Linux-only test skipped**. Covers
  data/receipt restoration, activation failure, commit/repeated recovery,
  interrupted rollback, bounded/refused backups, safe setup cancellation,
  root-entrypoint refusal and actual APK identity/hash predicates. Windows runs
  do not exercise Linux directory fsync or real protected-file ownership.
- `test_android_provisioning.py`: **11 tests OK, 5 Linux-only tests skipped**.
- Six JavaScript-controller checks pass for default Cancel, unsubscribe,
  update Back/cancel, busy cancellation, setup/renderer selection and Menu
  update-check access. This does not claim QML rendering or installed focus.
- Native tests extend `downloadedAndroidManifestPersistsAndRejectsTampering`
  with preference persistence, write-failure rollback, opt-out/late-result
  suppression and atomic failed-update registry/version preservation.
- Evidence: `build/store-lifecycle-checks/{fdroid-tests,lifecycle-tests,
  android-tests}.log` and `controller-checks.json`.

The other agent must run registered `store_lifecycle` / `fdroid_store`, native
tests and full CTest, then installed remote setup/auth/progress/cancel/retry,
protected snapshot/restore/crash recovery, real newer APK update, unsubscribe
across restart/reinstall, two-profile isolation, offline denial, exact focus and
standing visual/Generator journeys through the bounded VM runner. In
particular, native authentication availability/remote operability and protected
Android ownership/backup permissions are installed acceptance requirements,
not established by source or mocks. T29 remains **1/8 = 13%**; sum **860 / 27**,
Overall **32%**. No broad task gate closes from these source changes.
