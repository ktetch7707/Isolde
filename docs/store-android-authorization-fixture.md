# Android transaction authorization fixture — explicitly approved

Status: user explicitly selected **I approve this temporary VM polkit rule** on
November12 after the full rule scope and privilege change were stated. This
supersedes the earlier rejected generic agreement. The exact scoped fixture is
implemented only in the guest acceptance helper, with independent root expiry,
per-run installation/removal receipts and runner cleanup before service restore.
The native-auth fallback remains available. Product installation and
ordinary Store operations retain native administrator authentication.

The proposed VM fixture authorizes only the installed root-owned
`/usr/lib/isolde-android-setup/android-setup.py` entrypoint's `prepare` and
`recover` transaction commands for one fresh test UUID and its exact private
disposable UUID's exact data/activation paths. Evidence/configuration remain in
`/tmp/isolde-components-acceptance-store-<random>`; Android data uses the matching
`/var/tmp/isolde-components-acceptance-store-<random>/data` because production
rollback requires the same filesystem as protected `/var/lib` backup storage.
The policy binds those exact paths and grants no other location. It
requires the active local Isolde-Test user. It authorizes no setup/root-job,
arbitrary executable, shell command, normal profile or other user's path.
The production entrypoint continues validating caller UID, path ownership,
directory descriptors, bounds, rollback and durable activation tokens.

The fixture would temporarily skip the OS password prompt for those two exact
commands so actual Store install/cancel/failure/rollback can be automated.
This changes privileged authorization; the explicit approval above covers it.
It is never part of the product package.
The maximum lifetime is15 minutes. A bounded test cleanup must remove it before restoring normal Isolde service;
a separate expiry cleanup must remove it even if the test process crashes.
Install/removal, UUID, exact command strings and cleanup outcome must be logged.
No test may pass while cleanup is unverified. Normal Android profile data,
installed runtime and host security settings remain outside the fixture.

Alternatives: use the guest's ordinary native administrator approval for each
transaction, or retain authorization-denied/native boundary coverage without
claiming positive installed Android transaction acceptance. No password may be
requested in chat, inferred, stored or entered by the agent.
