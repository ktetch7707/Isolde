# Automatic VM test maintenance

User-authorized runner-only change: ordinary `tools/vm-test.ps1 Test` owns
low-space maintenance. No preliminary Status/Configure call is needed.

- The existing connection measures free space. Below 3 GiB, the host verifies
  saved workspace source archives and sends one bounded receipt (up to 1,024
  runs). Healthy disks skip this work. Host backups are never deleted.
- The locked guest first verifies/extracts the source and runs its safety tests,
  then uses the same maintenance implementation as explicit artifact Configure.
  Checked obsolete payloads, recoverable source copies, backed-up archives and
  disposable caches can be reclaimed. Logs/screenshots/manifests, profiles,
  installed releases, current/recent/pinned candidates and the incremental cache
  remain protected. A newly requested `-ReuseFullRun` base is pinned immediately.
- Cleanup aims for 3 GiB, but Test proceeds above its existing 2 GiB minimum.
  `guest-test-cleanup.json` records before/after/reclaimed bytes and both limits.
  Unsafe paths, mounts, changed bytes and insufficient safe space remain errors.
- CTest stops after the first failing registration, preserving its error rather
  than spending the rest of the full-suite budget on unrelated work.

Validation:

- Workspace PowerShell backup contracts pass: correct archive, incomplete
  snapshot, unsafe IDs, bounds, corrupted bytes, junction/ancestor redirection,
  and no host archive modification.
- First VM run `vm-auto-cleanup-20261107-a` caught a real retention bug in a new
  regression test before any maintenance. Directory mtimes change during
  pruning; source archive retention now uses stable archive upload timestamps.
- `vm-auto-cleanup-20261107-b`: 92/92 safety tests, real automatic cleanup and
  1/1 focused component registration pass. Reclaimed 292,745,216 bytes; 2.60 GiB
  available, so execution correctly continued below the optional 3 GiB target.
- Final `vm-auto-cleanup-20261107-c`: 92/92 safety tests and 1/1 component
  registration pass, including the newly requested reuse-base protection.
  Actual low-space preflight again completes in the same Test invocation.
  Source digest `062143f74198a00daae876c1b091140f4fa0ca7cca5d5934dbc2509b84aabb73`.
  Normal service remains active/running with zero restarts; current/previous
  installed release links match the preceding run.
- The CTest safety test runs a deliberately failing tiny fixture and proves its
  later marker-writing test never executes. This is a runner regression, not an
  application full-suite claim. `git diff --check` and PowerShell parsing pass.

Evidence is under `build/vm-oracle/<run>/report/`. Only harness/docs changed;
application full-suite repetition is unnecessary under the execution contract.
The earlier salvage full-suite failures remain open. No task acceptance credit:
sum 1548, Overall 57%. Commit uses the already documented command-scoped bypass
of the broken local hook; no hook, Git setting or host toolchain was changed.
