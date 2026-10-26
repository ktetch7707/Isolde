# T30 Games implementation checkpoint — 2026-10-26

Status: implementation started at the user's explicit request. T30 dependencies T12/T21/T22/T26 remain open; reuse their existing interfaces without granting acceptance credit or taking over the active Store slice. No Games software checkbox is complete.

## Capability decisions and sources

| Route | Primary evidence / decision | Still required |
| --- | --- | --- |
| Profile catalog | Qt SQL SQLite driver, separate database per profile next to its layout. Qt SQL is an explicit T30 catalog dependency; no new service. Transactions and FULL synchronous mode; unknown existing schema refused without automatic migration. | Distribution must include Qt SQL/SQLite plugin; complete catalog schema, backup/restore, edition/source/activity tables and migrations. |
| PSP | Candidate PPSSPP as a managed process, using documented fullscreen and frontend-exit options. [Upstream command line](https://www.ppsspp.org/docs/reference/command-line/) documents version-sensitive flags and newer memory-stick override. Do not confuse `--root` (host0 root) with save isolation. [Upstream license](https://raw.githubusercontent.com/hrydgard/ppsspp/master/LICENSE.TXT) includes GPLv2 and third-party notices. | Pin an actual Linux build, verify its supported options, include corresponding source/notices for distribution, choose a legal playable fixture, confirm private configuration/memory-stick paths and gamepad defaults. No runtime provisioned yet. |
| GameCube | Candidate Dolphin, supported user-directory override documented by [upstream](https://dolphin-emu.org/docs/guides/controlling-global-user-directory/). | Pin runtime/format/license manifest; game/controller fixture; measured software rendering/performance and separate target hardware qualification. Not yet advertised supported. |
| PS2 | Candidate PCSX2; [upstream requirements](https://pcsx2.net/docs/setup/requirements/) explicitly constrain CPU/GPU capabilities. | Actual VM/target capability probe, firmware import, license/format/runtime selection and playable fixture. Not yet advertised supported. |
| Steam | [Steamworks IPlayerService](https://partner.steamgames.com/doc/webapi/IPlayerService) is an authenticated network API, not the offline catalog adapter. Existing Big Picture remains unchanged. | Supported local installed-title discovery, account association and actual owned game/session detection remain unresolved; do not infer game completion from client bootstrap exit. |
| Local PC copies | First boundary supports explicitly selected registered native game apps or files opened by registered game runtimes with a declared open-file handler. Catalog input cannot provide program/argv/environment. | General native-folder registration, supported compatibility runtime, installer state/recovery and actual gameplay. |
| PC optical discs | Required dedicated source adapter, separate from Kodi/open-disc action. | Production device/mount protocol, virtual optical integration and physical H03. No optical capability claimed. |

Primary documentation is design evidence, not proof that a runtime is installed or that a title works. No commercial game, ROM, BIOS or firmware was downloaded for this batch. Legal playable fixture selection remains a blocker to real-game acceptance; security fixtures are inert files and never counted as gameplay.

## Implemented first batch

- Profile-local SQLite catalog foundation: manual registration, stable game IDs, literal title search, 50-row paging, 50,000-entry ceiling, Favorites/Recently Added, catalog-only removal and unknown-version rejection.
- Shared ComponentSchema validates registration and identity-only `launch-game`. ActionDispatcher resolves current profile catalog at execution, rechecks registered runtime capability and canonical readable file containment, then uses existing SessionManager.
- Games is available as content in the existing Studio library component; shared page renderer/card/appearance are reused. The mouse-free add/details/favorite/remove flow uses the existing on-screen keyboard. No existing layout is replaced or migrated.
- Registration grants access only after review; discovery does not execute files. No auto-launch, shell command field, provider database mutation or game/save deletion is added.

This is deliberately not the complete catalog schema or accepted game runtime. The current manual source workflow uses on-screen path entry; a friendly bounded folder picker/import preview remains required. Emulator provisioning, edition selection, collections, activity, managed artwork, installers, discs and actual gameplay are still open.

## Budgets and evidence

Provisional software budgets: 50 records/page; 50,000 records/profile; catalog title 256 characters, platform 80, query 160, root/relative target 4,096; SQLite lock wait 1 second. Five/5,000 fixture targets: cached page under 1 second, literal title query under 300 ms after debounce, no whole-catalog QML model. Measure before acceptance; actual target hardware is not yet identified.

`games-20261026-a` stopped at sandbox-denied localhost SSH, before any guest build. The authorized runner was then run with network permission as `games-20261026-b`; results are recorded below after completion. Normal profiles are not seeded and no deployment is implied by component tests.

First full gate games-20261026-c: 32/33 suites passed. Catalog/security tests and existing regressions pass. games_remote_ui failed because its C++ assertion assumed a QJSValue while Qt returned a QVariantList; the remote add/review/cancel test passed. The assertion now handles both Qt representations; games-20261026-d verifies the correction. No real gameplay claimed.

Final batch verification: games-20261026-d passes 33/33 full CTest suites. Studio, lounge and Pastel functional visual journeys pass; all 28 captures reviewed together in build/vm-oracle/games-20261026-d/review-sheet.png. Lounge recreation/reset remains functional; Pastel retains illustrative/missing-art and content acceptance gaps, and Studio frames retain visible guest desktop borders. No broad visual/design acceptance or actual Games gameplay claimed. Normal layout hashes unchanged and Isolde restores active with zero restarts. No deployment performed. Logs: build/vm-oracle/games-20261026-d/report/ctest.log; screenshots: its visual/ directory. T30 remains 0/9, Overall 32%.

Relink batch: adds remote Edit / relink source with a complete review draft, title/platform editing and atomic catalog update. Stable identity/Favorites survive a relink; invalid or duplicate targets roll back; source/save files are untouched. games-relink-20261026-a component_contracts passes, including gamesRelinkPreservesIdentityAndRollsBack. Targeted C++ verification only; this new edit flow still needs full QML/installed acceptance. T30 stays 0/9 and Overall 32%.
