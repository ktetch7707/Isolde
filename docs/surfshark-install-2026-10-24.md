# Surfshark guest installation — 2026-10-24

The user explicitly requested installation of Surfshark inside Isolde-Test. The Windows host is outside this scope.

`config/vpn-providers.json` pins the reviewed official installer SHA256 `37f435b52916dadea289bf3f26aba010237f7266b55e52353e19f3daac4cda3d` and repository signing fingerprint `D00FDC3221233215271F88DC1DB7590E83C8F643`. The provisioner never executes the vendor script: it extracts/verifies the public key, writes a scoped `signed-by` repository, installs `surfshark` through apt and verifies dpkg status/version. No existing package is removed. No host software or VPN settings change.

Reproducible entry: `tools/vm-test.ps1 Configure -RunId surfshark-20261024-full-l -ConfigureTarget surfshark`, requiring this tested/deployed snapshot and the guest administrator prompt. The control app launches in the guest desktop, outside private media-app sandboxes, because its daemon manages the guest's network. Credentials remain in its private interactive flow; no credential data is collected.

Evidence: `build/vm-oracle/surfshark-20261024-full-l/report/`.

- `ctest.log`: **28/28 passed**, 141.88 seconds. The default-app registration includes 18 provisioner regressions, including explicit guest/admin restriction, changed-installer refusal before trust writes, scoped repository trust and no vendor-script execution/package removal. Oracle self-test: **23/23 passed** (0.750 seconds).
- Source digest `421fa71bb633ac33e5f6d0f1968474e75c87e4e20eb3d632dae880ae27072ed3`.
- Deployed release `d3ce3684402c9a1e0f2510d9`, previous `943b344f27dfd6fe4e993246`.
- `surfshark-install-result.json`: **Surfshark 3.14.0-7904 installed**, reviewed installer/key match, `loginVerified: false`, `tunnelVerified: false`.
- Configure finishes successfully and opens the app. Status observes `surfshark.Surfshark`, PID 60233. Isolde is active/running, PID 60212, zero restarts. Household and Studio Demo layout hashes remain byte-identical to their pre-install values.

The user-visible post-install prompt is the guest's old locked login keyring, reporting an incorrect unlock password. This is distinct from the administrator authentication, which succeeded. Password recovery can leave the old keyring encrypted under its previous password. The user is advised to cancel that prompt; no keyring is reset, deleted, inspected or weakened, and no authentication dialog is automated. Surfshark login, tunnel/region verification, automatic per-app switching and the rejected Home graphical review remain pending.

Official provider installation documentation: https://support.surfshark.com/hc/en-us/articles/5067279648146-How-to-set-up-Surfshark-VPN-on-Linux
