# Actual App Store integration plan — verified Android repositories and Chromium web apps

**Status:** Implementation authorized by the user's “continue” request. The source decision below supersedes the Google Play/Chrome extension proposal in the historical sections.

Backlog reconciliation (2026-10-25): [T29](agent-backlog.md) owns the accepted real sources, remote bridge, Android/SmartTube foundation and complete lifecycle/playback gates; T12 owns the advertised default-app support matrix and T31 final software acceptance. [S1 software scope](first-product-software-scope.md) includes this work before hardware qualification. Sections 1–11 below retain the original proposal for provenance; their Google credential/GAPPS/extension architecture and unanswered source questions are superseded by the accepted decision, not additional requirements.

## Accepted source decision — 2026-10-25

The user selected **web apps and streaming sites, with Add by URL**, and **account-free verified Android stores such as F-Droid**. Implement F-Droid repository browsing/search and verified APK installation inside Isolde; Chromium covers web applications, not Chrome Web Store extensions/themes. Do not collect a Google password, add GAPPS, scrape Play Store credentials, or claim access to every Play Store app. Google distinguishes web apps from extensions: https://developer.chrome.com/docs/apps/migration. F-Droid's signed entry/index APIs are documented at https://f-droid.org/docs/All_our_APIs/.

The user also requests a remote-control bridge for Chromium apps. The implementation must cover existing externally launched Chrome web apps as well as the embedded web view: directional focus, visible selection, Confirm activation, text entry, scrolling/pointer fallback, Back and universal Home return. Verify with an actual owned browser session and local input fixture; no host browser control. Account login and subscription playback are independent acceptance gates.

The intervening coder committed provider scaffolding (`9aaa8d4`) and a Settings section (`a6637e7`). These are not evidence of a live repository: the original code contains invalid Qt Network signals, a missing Settings component and a download path that publishes installation without installing the downloaded bytes. Repair those issues while preserving the useful abstraction. T29 remains 0% until its explicit gates have evidence. The existing Store lifecycle release `014674ec29d7dfb787fd8ca9` has separate passing acceptance after the authenticated Waydroid selector repair; the new source has not yet been verified.

## 1. Goal

Let Isolde's Store browse and download **any app available on the real Android and Chromium app stores** (Google Play Store for Android APKs; Chrome Web Store for Chromium extensions/web apps), then install them through Isolde's existing Android (Waydroid) and Chromium (web-kiosk) pipelines, with the same profile isolation, confirmation, remote-first UX, and offline-first degradation as the current curated catalog.

## 2. Current state (baseline)

| Area | What exists | Files |
|---|---|---|
| Store surface | Remote-first QML store: platform selector (Android / Chromium), search, details, Install/Open/Uninstall with explicit confirmation | `qml/AppStoreView.qml`, `store/index.html` (legacy Chromium preview) |
| Catalog backend | `AppCatalog` loads `config/catalog.json` (platforms + package list), installs/uninstalls, tracks state atomically | `src/app/AppCatalog.{h,cpp}`, `src/app/AppRegistry.{h,cpp}`, `src/app/AppDefinition.h` |
| Catalog data | Curated local manifests only (SmartTube, Plex, Netflix, etc.); schema v1, pinned versions, SHA-256 | `config/catalog.json`, `config/packages/*/manifest.json` |
| Android runtime | Waydroid 1.6.3 VANILLA, SmartTube APK pinned + checksummed, admin guest provisioning, profile-scoped `XDG_DATA_HOME`/userdata | `config/android-runtime.json`, `tools/provision-android.py`, `tools/android-session.py` |
| Chromium apps | `web-kiosk` presentation mode, HTTPS launch URLs, `allowedOrigins` allowlist, WebEngine containment | `config/packages/plex/manifest.json`, `docs/packages.md` |
| Trust boundary | Local compiled catalog is not a network marketplace; remote repositories require signed metadata, verified hashes, consent, rollback, trust policy | `docs/packages.md` §Trust boundary |

There is currently **no Play Store, no Chrome Web Store, no remote search, no APK/CRX download pipeline, and no store credentials** anywhere in the tree.

## 3. Feasibility assessment (research required before implementation)

### Android — Google Play Store
- **No official public API** exists for browsing/searching/downloading Play Store apps. Official distribution requires a Google account, a registered/device-certified Android device, and Google Mobile Services (GMS). Isolde's Waydroid VANILLA image has no GMS by default.
- **Unofficial client libraries** (e.g. `googleplay-api-python`, `googleplay-api`) reverse-engineer the Play Store protocol. They require a real Google Account username/password (or auth token), perform device registration, and are against Google's ToS when automated. Credentials must never be hard-coded or stored in plain config.
- **APK mirrors** (APKMirror, etc.) are HTML-scraped, unauthenticated, and fragile — not a trustworthy source without independent signature/hash verification.
- **F-Droid** offers a public, index-based repo of FOSS APKs; it is the most technically straightforward "real store" source, but it is not "any app on the Play Store."

**Conclusion:** Isolde cannot honestly promise "any app on the Play Store" without Google account credentials and device registration. Play Store access should be an **opt-in online feature** using an unofficial API **only if** the user supplies credentials at runtime (never stored by Isolde), with F-Droid/mirror support as optional, verified-source additions. "Any app available" is limited to what the chosen source actually exposes and what the Waydroid runtime can run (no GMS-dependent apps, no DRM/Widevine apps, no ARM-only APKs on x86 guest).

### Chromium — Chrome Web Store
- Free extensions/themes can be downloaded as CRX by extension ID; the public store search is HTML/JSON scraping of `chromewebstore.google.com` (fragile, no official API, requires consent to Google ToS).
- Paid extensions require Google authentication.
- Isolde already runs web-kiosk apps from arbitrary HTTPS URLs with `allowedOrigins`. The practical "Chromium store" equivalent is letting the user add any web app URL as a Chromium tile, plus optional Chrome Web Store CRX fetch by ID.

## 4. Design principles (must be preserved)

From `AGENTS.md` and `docs/unified-master-plan.txt`:
- **Offline first:** Web OFF issues zero external requests. Remote store access is opt-in online and degrades to the curated local catalog.
- **Profile isolation:** downloads/installs affect the active profile only; no cross-profile reads/writes; teardown invalidates sessions.
- **Atomicity:** validate → download to temp → verify hash → fsync → rename → rollback on failure. Keep deterministic write-failure regression passing.
- **Validate at boundary:** reject unknown sources, unsafe IDs/paths, overflow/collision, non-managed artwork; surface via `errorString()` with rollback.
- **No new dependencies/frameworks/side services without task scope.** Reuse `AppCatalog`/`AppRegistry`/`ActionDispatcher`, `KodiService`, `InputManager`, `SessionManager`; Qt Network only (already a dependency).
- **Remote-first:** every workflow completes with D-pad + Confirm + Back + Menu/Search only; focus always visible and restored.
- **Explicit consent:** no silent install, no stored credentials, no credentialed URLs, passwords masked if entered.
- **Kodi owns media:** store apps do not become a parallel media DB or playback path.

## 5. Architecture

Add a thin **store provider** layer behind `AppCatalog` so the UI never talks to a network store directly:

```
AppStoreView.qml (UI)
    → AppCatalog (install/uninstall/state, unchanged pipeline)
        → StoreProvider (abstract: search, get artifact URL + hash, provenance)
            → PlayStoreProvider   (opt-in, runtime credentials, unofficial API)
            → FdroidProvider      (optional public index, verified APKs)
            → ChromeWebStoreProvider (CRX by ID, opt-in)
            → LocalCatalogProvider (existing curated manifests, offline default)
```

New data models:
- `StoreSearchResult { id, name, publisher, category, iconSource, type, store, storeId, version, sizeBytes, provenance, downloadUrl, sha256, networkRequired, consentText }`
- `StoreDownload { id, state, progress, errorString }`

`StoreProvider` returns **metadata + a verified download URL + SHA-256**. `AppCatalog` owns downloading (Qt Network to temp file), hash verification, atomic move into the profile's package store, and the existing Waydroid/web-kiosk install paths. Network code lives in `AppCatalog`/a new `src/app/StoreDownload.*` — not in QML.

Credential handling: if a provider needs user credentials (Google Account), they are collected **at runtime in a remote-operable dialog**, held in memory only, never written to config, masked in logs/UI, and cleared on Back/cancel.


## 6. Implementation phases (vertical slices)

### Phase 0 — Research & feasibility (no code change, this plan)
- Confirm which store sources are acceptable to the user (Play Store w/ credentials vs F-Droid vs manual APK URL).
- Confirm guest capability: Waydroid without GMS, x86 APKs only, network access in the VM.
- Record decision in this doc; do not implement until the user confirms the source list.

### Phase 1 — Provider abstraction + offline-safe search (no network by default)
- Add `StoreProvider` base class + `LocalCatalogProvider` wrapping the existing catalog (offline default, zero external requests).
- Add `StoreSearchResult` model exposed to `AppStoreView.qml` alongside existing entries.
- Add a Settings toggle `Online store search` (default OFF). When OFF, the Store shows only curated local packages and issues zero external requests.
- Unit tests: provider rejection of unknown source, offline behavior, result model validation.
- **Gate:** `ctest` passes; Generator Test (disconnected) still passes with zero external requests.

### Phase 2 — Android real-store source (Play Store / F-Droid / mirror)
- Implement chosen provider(s): search → result list with `downloadUrl` + `sha256`.
- Add APK download to temp file, SHA-256 verification, atomic move into profile package root; reuse existing `android-session.py` install path (bounded helper, checksum-verified, profile-isolated).
- Add runtime Google Account credential dialog only if Play Store provider is selected; mask input; never persist.
- Regressions: download failure, hash mismatch, cancelled download rollback, unsupported ABI/refusal, GMS-required app warning.
- **Gate:** real APK downloaded, verified, and installed into an active profile in the VM; uninstall removes it; layout hashes unchanged.


### Phase 3 — Chromium real-store source (Chrome Web Store / web apps)
- Implement Chrome Web Store lookup by extension ID (opt-in, public CRX download URL + hash) and/or "add any web URL as Chromium app" flow.
- Download CRX to temp, verify hash, atomic move; install as `web-kiosk` package with `allowedOrigins` derived from the manifest or user entry.
- Regressions: invalid extension ID, download failure, origin allowlist enforcement.
- **Gate:** a Chromium app from the real store installs and launches in the VM; offline default unchanged.

### Phase 4 — Store UX integration (remote-first)
- Unified platform header: Local / Android store / Chromium store picker (D-pad operable).
- Search across the selected active provider; show provider badge + provenance on each card.
- Download progress, Install/Open/Uninstall confirmations (default Cancel), busy states, error capture, focus restoration to the same card after install/launch/close.
- **Gate:** all Store workflows complete with D-pad + Confirm + Back only; `tst_isolde`/store acceptance suite passes.

## 7. UX requirements (remote-first TV)

- Every workflow: D-pad + Confirm + Back + Menu/Search only; no mouse dependency.
- Search opens an on-screen keyboard; results re-filter live; focus preserved.
- Install/Uninstall require explicit confirmation dialog defaulting to Cancel; Back restores all affected items; preview writes nothing.
- Busy state shows progress + "Installing %1… Please wait"; failure shows actionable `errorString()` and "Return Home and retry."
- Store card shows: name, publisher, category, version, store badge (Play Store / Chrome Web Store / Local), provenance, state badge (ready/failed/needs-setup), Install/Open/Uninstall action.
- Up from first row reaches Settings/Edit Home; empty Store still reaches recovery; no autoplay/ads/feeds in default.


## 8. Security & trust boundary

- All payloads verified by SHA-256 before installation; mismatch → delete temp, error, rollback, never install.
- Downloads bounded in size and time; temp files in profile-private dir with `0o700`; fsync + rename.
- No credentials in `config/` or committed to git; runtime credentials in memory only, masked.
- No arbitrary Exec/shell recipes, no root grants, no stored layout changes from store operations.
- Apps marked `networkRequirement: internet` are dimmed/disabled when offline; no claim that DRM/Widevine/GMS-dependent apps work.
- Unknown/unsafe IDs/paths rejected; unknown store source rejected, never clamped silently.

## 9. Tests & verification

- Extend `tst_components.cpp` / `tst_isolde.cpp` with: provider rejection, download+verify+rollback, hash mismatch, write-failure regression, profile isolation, offline-zero-requests.
- Store acceptance: `tools/store-acceptance.py` extended for real-store Android + Chromium install/launch/return/uninstall.
- Run `ctest --test-dir build --output-on-failure`; both `tests/visual/home-reference` and `tests/visual/home-pastel-reference` recreation/reset pairs.
- VM evidence under `build/vm-oracle/...` and `/tmp/isolde-*`; no host writes outside the workspace.



## 10. Open questions / risks (decisions needed before coding)

1. **Store source selection:** Play Store (requires user Google credentials at runtime, unofficial API, ToS risk), F-Droid (FOSS only, simple public index), or manual APK/CRX URL by the user? Which does the user want?
2. **Credential collection:** Is a remote-operable Google Account/password entry acceptable, or should Play Store be out of scope?
3. **Guest networking:** Does the Isolde-Test VM have unrestricted outbound HTTPS to play.google.com / chromewebstore.google.com / repo.f-droid.com?
4. **ARM vs x86:** The guest is x86_64; ARM APKs cannot run. Filter results by ABI.
5. **GMS-dependent apps:** Many Play Store apps require Google Play Services; Waydroid VANILLA lacks them. These must be flagged unsupported, not silently failed.
6. **Backlog placement:** This work is outside the current T15-T24 Page Studio priority. A new task ID (or tasks) would need to be added to `docs/agent-backlog.md` with `%` accounting before implementation begins.

## 11. Done criteria (evidence required before 100%)

- Plan reviewed and source list confirmed by user (this document).
- New task(s) added to `docs/agent-backlog.md` with `%` accounting.
- Offline default: Generator Test passes disconnected, zero external requests.
- Real-store Android app: download → verify → install → launch → return → uninstall in VM, with evidence frames.
- Real-store Chromium app: same journey in VM, with evidence frames.
- Full `ctest` green; both visual fixtures pass; commit/push to `agent-backlog-progress` with `git log -1 --oneline && git status` clean.
