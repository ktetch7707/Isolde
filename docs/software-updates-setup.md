# Isolde software updates: human setup and remaining work

## What exists now

Isolde checks GitHub's latest stable release when the active profile has Web ON.
Web OFF stops checks and cancels an active request. On supported appliance
and Ubuntu desktop package installations, a newer release with the expected
asset names produces a small badge on Settings controls and a Software Updates
entry under System & Support.
Dismissal is saved for the device and the badge returns for a newer version.
The installed version and discovered candidate are shown in Settings.

**This is discovery only.** Release information and asset names are not yet
signature-verified. Isolde cannot download or install an update from this
screen. A badge is a candidate, not proof that a package is trusted or usable.
No release workflow has been added and GitHub currently has no published
release. Do not publish a real update as though installation works.

For now discovery recognizes the active `current` release of the existing
per-user appliance installer, or the desktop package at
`/opt/isolde/bin/isolde`, on Ubuntu 26.04 and x86-64 or ARM64. Source builds,
other prefixes and other OS versions are reported as unsupported. Architecture
recognition is not an assertion that both architectures have been built or
tested. Desktop package discovery is not package installation or signature
verification.

The desired desktop update replaces Isolde's application package, not Ubuntu
or the user's home directory. The package builder has a fixture-based upgrade
test for preserving a home file, but real Isolde profile migrations, package
dependencies, interrupted updates and clean desktop upgrades have not been
validated. Keep backups until those paths are proven.

## Decisions and setup needed from a human

1. **Choose the release targets.** The first easy-install target is an Ubuntu
   26.04 desktop package, installed with administrator authorization. Confirm
   which architectures will actually be built, tested and supported, and
   whether other Linux versions should be supported later.
2. **Choose the signing-key custody.** Designate the release maintainers and
   a protected signing environment. Decide how an offline recovery/rotation
   key is held and how access can be revoked. Do not put a private key in the
   repository, an app package, an issue, a log or this document. Do not create
   or upload a production key until the signing format and workflow exist.
3. **Configure GitHub protection once the release workflow is implemented.**
   In `ktetch7707/Isolde`, protect release tags and the release environment,
   restrict publishing permissions and require release approval. Provide the
   signing credential through the approved protected mechanism; never paste it
   into chat. The application must ship only the corresponding public trust
   material.
4. **Approve initial distribution.** The first version with a secure updater
   must be installed manually on existing devices; older binaries cannot
   receive a capability they do not have. Decide how those devices obtain and
   verify that first release. Web OFF intentionally prevents update checks.

## Work still required before updates can ship

- Centralize versioning across `VERSION`, CMake, the executable, release tag
  and artifact metadata. The current project uses four numeric components.
- Create tested release builds and a publishing workflow. The discovery
  contract currently expects stable tags such as `v0.0.1.3` with
  `isolde-0.0.1.3-linux-amd64.tar.gz` (or `linux-arm64`),
  or a desktop package named `isolde_0.0.1.3_amd64.deb` (or `arm64`),
  plus `update-manifest.json` and `update-manifest.sig` on the same release.
  Finalize this provisional format before publishing; GitHub asset names
  alone do not authenticate anything.
- Define and verify a signed manifest binding version, OS, architecture,
  filename, size, hash and channel; implement key rotation and rollback/replay
  defenses. Add bounded downloads, cancellation and safe archive extraction.
- Implement an independent per-user installer that re-verifies the downloaded
  release, uses the existing installer lock and release layout, and waits for
  explicit user confirmation. It must not execute arbitrary paths or run the
  GUI as root. The current installer accepts optional payload signatures; that
  is not sufficient for an automatic update trust boundary.
- Design the desktop package update path separately: a package-manager-owned
  installation under `/opt/isolde` must not be modified by the per-user
  appliance updater. Provide a trusted, explicitly
  authorized package-manager update flow before promising Settings updates
  for desktop users.
- Add durable activation journaling, startup/health checks and tested
  recovery. The existing previous-release switch does not undo data
  migrations; do not promise rollback until compatible data recovery is
  implemented and tested.
- Verify the actual installed VM journey, full CTest suite, remote navigation,
  customized Settings buttons, offline/Web OFF, rate limits, bad signatures,
  incompatible releases, low disk space, interrupted installation, failed
  restart and user-data preservation. Test each supported architecture.

Until those items are complete, the user-facing page intentionally provides
only checking and dismissal, not a Download & Install action.
