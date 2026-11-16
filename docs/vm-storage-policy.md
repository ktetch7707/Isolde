# Bounded guest storage

The November 13 repair replaces indefinite installed-release retention. The
50 GiB guest root had 148 obsolete releases and six obsolete staged payloads;
verified removal recovered space from about 1.7 GiB to 13.1 GiB. No host disk,
snapshot, profile, source receipt or incremental build cache was removed.

Activated policy runs automatically before ordinary Test and Deploy. Retain
three newest releases and staged payloads, plus current/previous, recovery
service, live process, stopped-profile and reachable candidate dependency pins.
Historical receipts remain evidence but do not pin executables indefinitely.
Unknown staged payloads remain untouched. Modified identities, unsafe paths,
mounts or redirects refuse maintenance. Installation locking and a second
identity/current/previous/live check precede deletion.

Explicit audit: `Configure -ConfigureTarget storage-capacity -RunId <id>`.
Apply: add `-ApplyStorageRetention`. Both use verified runner self-tests.
Each produces storage-retention-plan/result.json. The existing 2 GiB test
minimum and low-space cleanup remain in force.

Evidence: `build/vm-oracle/vm-space-20261113-c/report` records actual cleanup;
`vm-space-20261113-e/report` records an idempotent zero-removal repeat at
12.8 GiB free and 129 passing safety checks. Safety coverage includes ten
simulated deployment cycles with bounded release counts and rollback/live
protection. Failed d was a Python method-placement syntax error, corrected
before e; it executed no maintenance.

Test-tree .trackerignore markers use the guest's existing Tracker3 policy,
without changing global settings. The interrupted native-build indexer lease
was restored; native configuration now restores it even after failure.
