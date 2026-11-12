# Android transaction authorization fixture — approval pending

Status: proposed only. Automatic approval review rejected its implementation;
no authorization rule has been written or installed. Product installation and
ordinary Store operations retain native administrator authentication.

The proposed VM fixture authorizes only the installed root-owned
`/usr/lib/isolde-android-setup/android-setup.py` entrypoint's `prepare` and
`recover` transaction commands for one fresh test UUID and its exact private
`/tmp/isolde-components-acceptance-store-<random>` data/activation paths. It
requires the active local Isolde-Test user. It authorizes no setup/root-job,
arbitrary executable, shell command, normal profile or other user's path.
The production entrypoint continues validating caller UID, path ownership,
directory descriptors, bounds, rollback and durable activation tokens.

The fixture would temporarily skip the OS password prompt for those two exact
commands so actual Store install/cancel/failure/rollback can be automated.
This changes privileged authorization and therefore requires explicit approval
under the automatic review decision. It is never part of the product package.
The maximum lifetime is15 minutes. A bounded test cleanup must remove it before restoring normal Isolde service;
a separate expiry cleanup must remove it even if the test process crashes.
Install/removal, UUID, exact command strings and cleanup outcome must be logged.
No test may pass while cleanup is unverified. Normal Android profile data,
installed runtime and host security settings remain outside the fixture.

Alternatives: use the guest's ordinary native administrator approval for each
transaction, or retain authorization-denied/native boundary coverage without
claiming positive installed Android transaction acceptance. No password may be
requested in chat, inferred, stored or entered by the agent.
