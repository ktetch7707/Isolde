# T29 — F-Droid browsing and search

The Store now registers a profile-scoped `FdroidProvider` in the normal runtime
and displays `AppCatalog.allEntries`. Creating the provider makes no external
requests. Online browsing remains OFF by default.

In the Android Store, choose **F-Droid search: On**. Search filters the repository;
Menu reaches the browsing controls, with Left/Right selecting the toggle,
Previous and Next. Pages contain at most 50 online results, merged with the local
catalog. Install retains its explicit confirmation. Source and compatibility
details appear alongside the app. Switching OFF kills the provider request,
clears online results and cancels any APK download. Errors allow a new search to
retry; the local catalog remains usable.

## Implementation and trust

- `FdroidProvider` runs the packaged `fdroid-store.py` helper through a bounded,
  asynchronous QProcess. It never starts Android or installs a package itself.
- The helper downloads the official `entry.jar`, verifies its actual PKCS#7
  signer against F-Droid's pinned SHA-256 certificate fingerprint, validates
  the SHA-256 JAR manifest chain, and checks the entry's protocol version and
  age. It then downloads `index-v2.json` and verifies the signed size and SHA-256.
  It does not use the unsigned display/search API or legacy SHA-1 index.
- HTTPS requests stay inside the official repository. No repository-directed
  mirror, icon, screenshot or arbitrary executable is fetched. APKs continue
  through AppCatalog's existing bounded SHA-256 installer path.
- The cache belongs to the active profile and is verified again on every use.
  Search and pagination reuse a recent cached entry/index without network
  requests. Expired entries refresh on the next explicit online request.
- App versions are selected against read-only Android image SDK/ABI/feature
  data. Unknown runtime details disable installation with an explanation.
  Unsupported ABI/SDK/features remain visible with an explanation. Prerelease,
  known-vulnerable, disabled-signature and expansion-file builds are excluded.
  This compatibility screen does not certify app usability or playback.
- Existing Android appliance setup now ensures the OpenSSL command is present
  alongside its existing APK validation tools. No host software was installed.

Trust references: [F-Droid APIs](https://f-droid.org/docs/All_our_APIs/) and
[official signing key](https://f-droid.org/docs/Release_Channels_and_Signing_Keys/).

## Verification and handoff

No VM actions, builds, deployment or installed GUI checks were run, as requested.
The host has no configured Qt/CMake toolchain; native tests were added for the
other agent rather than claiming a compile pass.

- `tests/test_fdroid_store.py`: **12/12 passed**, including real generated
  PKCS#7 signatures, pinned-signer rejection, cryptographic tampering, manifest
  and index tampering, stale/future/unknown-protocol rejection, cached hash
  revalidation, search/pages and compatibility filtering.
- `tests/test_android_provisioning.py`: **11 tests OK, 5 Linux-only skipped**.
- The official live SHA-256 entry and 68,377,634-byte v2 index were downloaded
  into the authorized workspace and verified. The index contained 4,866 apps;
  bounded searches and browsing returned real results. This is repository proof,
  not Android install/launch or VM acceptance. The synthetic runtime target used
  for parser inspection is not a measured Android runtime.
- Logs: `build/store-installer-checks/fdroid-tests.log`,
  `fdroid-live-check.log`, and `android-after-fdroid.log` in the same directory.
  Downloaded indexes are untracked build evidence, not committed artifacts.
- Added native regression
  `fdroidBrowsingStaysOfflineUntilOptInAndDropsLateResults` checks default-OFF
  behavior, query/page forwarding, cancellation, and late-result rejection.

Remaining verification: full Linux CTest (including registered `fdroid_store`),
actual Store loading/search/paging/cancel/retry/focus, initialized Android
compatibility detection, real-source install/open/return/remove, profile
isolation, offline Generator Test and standing visual fixtures. T29 stays
1/8 = 13%; Overall stays 32% (860 / 27 rounded). No broad gate is closed.
