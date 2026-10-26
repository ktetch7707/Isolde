# Games page — unified library and delivery plan

Original proposal: Solar Mini4, 2026-10-25. Reviewed and revised: 2026-10-25.
Status: planning only; no implementation, provider support or acceptance claimed.

Backlog integration (user-requested reconciliation, 2026-10-25): [T30](../agent-backlog.md) now owns G0–G6/G5a/G5b software delivery, and T31 owns final integrated software acceptance. [S1/H1/R1 scope](../first-product-software-scope.md) splits software completion from mandatory physical TV-remote/Xbox and optical qualification (H02/H03). This supersedes only the unmapped-task/hardware-first scheduling wording below; every physical acceptance requirement remains. No application/runtime implementation is authorized or completed by this planning edit.

## 1. Outcome and authority

Games should behave like Movies: a cover-driven personal library, operated from a remote and Xbox controller, with discovery, organization, details, edition selection, launch and exact return inside Isolde. Steam or an emulator may remain necessary behind a Play action. Opening Big Picture is a useful fallback, not proof of a unified library or direct game launch.

Requirements: [unified master plan, section 6](../unified-master-plan.txt), [execution order](../implementation-order.md), [backlog](../agent-backlog.md), and repository AGENTS.md. The master plan explicitly requires a local games catalog, manual metadata, collections, offline artwork and multiple editions. Kodi's prohibition on a parallel movie/TV database must not erase that distinct games requirement. Kodi remains authoritative for its media; storefronts/emulators remain authoritative for entitlements, installations, execution and saves.

This review authorizes documentation only. It does not reorder Page Studio, start a build, install emulators or authorize new providers/dependencies. When implementation is selected, map slices to an explicitly scheduled backlog task and synchronize dependencies/counts before granting credit. Do not invent a task ID or take over another coder's active work.

### 1.1 Decisive acceptance test — TV remote to Xbox controller, no mouse

User acceptance requirement (2026-10-25): assume the TV remote and Xbox controller are already correctly connected to Isolde. A user must launch a game through Isolde using only the TV remote, then actually play it using the Xbox controller. At no point may a mouse be necessary. Device pairing is a precondition, not an excuse to omit the end-to-end test.

Required witnessed journey for every advertised supported launch route (storefront, local copy and inserted PC disc):
1. Start on the normal installed Isolde interface with mouse/touchpad and physical keyboard unused for the entire journey.
2. With only TV-remote D-pad, Confirm, Back and Menu/Search, reach Games, find/select the title, open details, choose an edition if needed and select Play. Any required setup, install/disc prompt, launcher dialog, permission, login or error/retry step must also have a remote-operable route with visible focus and on-screen text entry where needed. Physical disc insertion/swapping is the expected physical action.
3. The launched game's start screen, menus and actual gameplay work with the Xbox controller. Demonstrate meaningful player input in a playable scene; a launcher, logo, sign-in screen or attract-mode animation is not a gameplay pass. Record the tested title/version/runtime and controller mapping.
4. Use the configured Return to Isolde binding, or exit the game with its controller UI, and regain the exact originating game card and scroll position. The remote remains usable after return; no desktop rescue, mouse click, window refocusing or terminal command is allowed.
5. Repeat the relevant canceled/failed launch and missing/wrong-disc recovery with the remote alone. Neither route may strand focus or require a mouse to dismiss a foreign window.

A title/runtime/installer that requires a mouse at any required step fails this gate and cannot be labeled supported for the couch workflow. Showing an honest unsupported message is necessary recovery, but does not pass the launch/play requirement. A mouse-emulation workaround is not the default remote navigation design. Optional desktop controls remain available without being prerequisites.

Retain an uninterrupted witnessed input/action record or recording, with milestone captures, actual gameplay and return evidence. Run physical-device acceptance in addition to automated QML/native input tests; simulated key events alone do not establish the TV-remote-to-Xbox handoff. Do not report universal game compatibility from a single tested title; report each accepted route and its supported title/runtime coverage explicitly.

## 2. Review findings and verified baseline

| Original proposal issue | Required correction |
| --- | --- |
| Calls launching Steam gamepad UI “no need to enter Steam” | Distinguish storefront launcher, direct game launch and successful gameplay. |
| Promises Steam library/recent/playtime/categories without an identified interface | Investigate each capability separately; absent or undocumented capability remains unavailable. |
| Rejects any games database and treats Saved Games/XDG state as a universal library fallback | Restore the master-plan catalog; manual registration is the dependable baseline. No assumed cross-platform history source. |
| Lists emulators, Game Pass, PlayStation and physical discs as working backends | Verify each adapter independently. Local PC copies and inserted PC discs are required scope under §3.1; online services remain deferred. A service tile is not an indexed or playable game. |
| Uses Kodi playback return for game lifecycle | Reuse native SessionManager return, with game-specific launch/session identity and owned-process proof. |
| Claims read-only enumeration but requires import, metadata, tags and history | Separate read-only external discovery from Isolde-owned catalog edits and derived display state. |
| Backend-first phases defer all UI and failure handling | Deliver tested end-to-end slices with visible behavior, retry and focus restoration. |
| Copies executable/path/argument data into page components | Store stable references; trusted adapters resolve launch targets at execution time. |
| Says Confirm launches, while master plan requires details | Default card Confirm opens details; prominent Play launches. A configurable quick-play policy may be added through shared schema/editor/preset. |

Repository inspection for this review:
- `config/default-apps.json` declares `gamesBackend: steam` and `/usr/games/steam` with `-gamepadui`; `config/default-home-layout.json` contains the Steam launcher.
- `src/app/AppRegistry.cpp` accepts `game-hub` and `game` presentation labels. Labels establish no discovery, lifecycle or rendering capability.
- `src/sessions/SessionManager.h` exposes `launch(appId)` and `launchAction(action)`, not a general per-game launch API. `SessionManager.cpp` validates actions and rejects an already active session; its `returnedHome` signal is already connected in `ActionDispatcher.cpp`.
- `NativeAppHost.cpp` starts a child process with separate program/arguments. Extending that process path does not prove ownership of an already-running Steam client or its game.
- `open-disc` handling depends on a configured handler; the action name does not establish physical-game support.

External Steam/emulator API behavior has not been verified in this documentation review. The capability investigation below must use current primary documentation and actual guest evidence before selecting an implementation.

## 3. Product scope and staged support

The first accepted product slice is one manually registered local game, shown on the real configured Games page, with details, safe launch, failure/retry and exact return. It must work disconnected with a preinstalled supported runtime and legally supplied fixture. Do not ship a static mock as completion.

Subsequent scope restores all master-plan features: background import, All Games, platform/franchise/tag filters, Favorites, Recently Added, Continue Playing, manual and smart collections, multiple editions, managed local artwork, catalog backup/export and 5,000-entry usability. Steam discovery follows demonstrated capabilities rather than blocking the local catalog/UI.

Required integrated emulator delivery is defined in §3.2; it may follow the first local-game slice but is not optional. Outside delivery scope: BIOS/ROM acquisition, cloud gaming, subscription entitlement discovery, DRM workarounds, save synchronization, save-state control, high-end emulator qualification and storefront account automation. Track these as deferred, not implicitly supported. Do not download games or firmware as test setup.

### 3.1 Local PC copies and inserted PC game discs — required scope

User requirement (2026-10-25): the unified library must support local game copies and inserted PC game discs, alongside storefront and emulated games. These are required delivery tracks, not optional future providers. Compatibility remains per title/runtime; this plan does not promise that every Windows disc runs on the Linux appliance.

| Source form | Required user journey |
| --- | --- |
| Already installed local PC game / portable game folder | Add game or folder → identify/select the supported entry point → choose an approved native or compatibility runtime → review → add to All Games and relevant shelves → Play → exact return. No Steam account or Steam import requirement. |
| Local installer copy | Select an installer from an authorized folder → compatibility/preflight and destination review → explicit Install → bounded progress/cancel/recovery → verify installed entry point → add playable installation to the existing edition. An installer file alone is not a playable installation. |
| Inserted PC game CD/DVD | Detect insertion through the supported OS device interface → show a non-modal configurable disc entry/notification → inspect details → offer Install, Play or Unsupported according to verified capabilities. Never run autorun/setup automatically. |
| Installed title requiring its original disc | Play validates the required disc → prompt Insert disc with Retry/Cancel → verify the selected edition/disc → launch through its approved runtime. Ejecting retains the catalog and installed files. |

Disc handling is a separate typed source adapter, not Kodi optical playback, a console-ROM adapter or proof supplied by the generic `open-disc` action. Track a media-set/edition reference, disc number/count and bounded observed volume identifiers separately from the transient drive/mount path. Volume labels alone are not unique identities or entitlement proof. Ambiguous media gets an explicit user association; unexpected replacement media cannot inherit an executable approval.

Use existing supported OS mounting/device mechanisms with narrow grants; avoid a new privileged helper unless separately scoped. Resolve the current device, mount and granted target again immediately before reading or launch. Treat mounted media as read-only input and distinguish no drive, no media, mounting, ready, wrong disc, unsupported format, permission denied and read error. Do not bypass encrypted/protected media, copy protection, activation or license checks. Disc imaging/ripping is outside this requirement; an existing authorized local image needs a separately verified format/mount capability before being advertised.

Installing is an explicit user operation, distinct from read-only discovery. Before installation show the chosen runtime, destination, estimated space when known, external activation/network needs and any shared account/save state. Keep writable installation/runtime state within an approved game destination or managed profile runtime; do not elevate an arbitrary installer or grant broad host filesystem access. Do not promise remote-only compatibility with an untested third-party installer: supported titles must demonstrate installation using the approved remote/controller workflow, otherwise clearly report the setup limitation before starting and mark that title/installation route unsupported for the mouse-free acceptance gate (§1.1).

Define installation states: selected → preflight → installing → verifying → ready, with canceled/failed/recovery outcomes. Stage new managed installations before publishing catalog references. For arbitrary vendor installers, record actual side effects and supported recovery rather than claiming transactional rollback of unowned files. Never overwrite an existing installation or remove saves during retry/uninstall. Uninstall only owned installation artifacts after explicit review; existing user-owned folders are detached from the catalog unless deletion is explicitly requested.

Multi-disc installation prompts for the next verified disc without discarding progress; cancellation and missing/wrong/unreadable disc remain recoverable. During play, disc removal reports capability-specific recovery and does not kill unrelated processes or corrupt catalog state. Eject is remote-accessible and refused/explained while the owned installer/game is using the drive. Disc insertion/removal never steals gameplay focus or launches a new session.

Native Linux execution and any Windows compatibility runtime are separate capability profiles. G0 must evaluate a supported existing runtime route, architecture, graphics/input, installer behavior, activation and disc access for each accepted fixture. A compatibility runtime or dependency is not installed by this planning edit. Offline acceptance uses a verified title that needs no external activation; online-only titles disclose that requirement and cannot satisfy the offline gate.

### 3.2 Integrated emulators — required product capability

User clarification (2026-10-25): selected emulators must be built into the Isolde product experience, not left as manually installed external prerequisites. Isolde provides managed runtime provisioning, configuration, game import, controller defaults, launch and return through its own interface. Reuse established emulator projects as managed backend processes; this is not a requirement to write new emulation engines or embed their windows in QML.

The normal journey is: Games → Add games → choose platform → select an authorized game folder → Isolde checks/prepares the supported runtime → review/import → select game → Play with the TV remote → actual Xbox-controller gameplay → Return to Isolde. Emulator desktop menus, terminal commands, manual configuration files and a mouse cannot be prerequisites. Necessary firmware selection, runtime repair, controller mapping and setup errors must be remote-operable through Isolde.

Runtime delivery must be repeatable and versioned: select vetted upstream packages/builds, record licensing and redistribution terms, pin versions and verify artifacts, include required dependencies, provide progress/cancel/retry and safe update/rollback, and preserve game files and saves. Use existing package/provisioning and SessionManager boundaries. Online runtime installation requires an explicit setup action; the offline appliance distribution must include its supported emulator runtimes and required redistributable dependencies so cold-boot local play does not trigger downloads. Non-redistributable firmware is a user-supplied prerequisite with a clear preflight/import flow; never call a platform ready until it passes that check.

Each runtime exposes a validated platform/format/settings capability profile. Provide polished defaults for video/aspect ratio, audio, Xbox bindings and safe exit, with supported options editable through the shared Isolde settings model. Settings generate typed backend configuration, not arbitrary executable commands. Keep runtime configuration, save files and save states profile-scoped wherever the backend supports it; verify ownership and shutdown flush before marking isolation or save preservation accepted. Do not silently reuse another profile's emulator configuration or saves.

G0 must produce a concrete supported-platform matrix. PSP is an explicit initial acceptance target from master-plan §6. GameCube and PS2 are named requirements to assess against target hardware; their inclusion and runtime choice require measured gameplay/controller compatibility, not an assumed universal emulator bundle. Select the actual emulator projects using current upstream documentation, supported Linux packages, licensing, command-line/configuration capabilities and hardware evidence. No emulator brand or number of supported systems is inferred as already agreed.

Emulator scope retention (user reminder, 2026-10-26): the named runtime work is broader than the two Ubuntu packages installed so far. Keep the original proposal's RetroArch route visible alongside the master-plan PSP target. Do not close T30.6 from Dolphin/PCSX2 provisioning or a native SuperTux test.

| Runtime track | Current state | Required remaining result |
| --- | --- | --- |
| PPSSPP / PSP | Not provisioned or integrated; no candidate in the guest's configured Ubuntu package index | Verified pinned delivery, private configuration/saves, Isolde setup/import/settings, actual PSP game input and exact return. This is the initial emulator acceptance target, not an optional follow-up. |
| Dolphin / GameCube and Wii | Pinned guest package, curated manifest and file-launch binding exist | Isolde configuration/controller/firmware-as-needed preflight, offline payload/update recovery and actual emulated gameplay/return for each declared platform. |
| PCSX2 / PS2 | Pinned guest package, curated manifest and file-launch binding exist | Remote-operable firmware/setup/configuration, offline payload/update recovery and actual PS2 gameplay/return. |
| RetroArch / selected classic systems | Named in the original base proposal; not provisioned or integrated | Declare and verify each core/platform/format/license separately, pin both frontend and core artifacts, and integrate setup, controller defaults, private saves, game launch and return through Isolde. Frontend installation alone never establishes console support. |

Additional emulators/systems from the user's original discussion must be recorded when identified; do not invent an agreed console/core roster or silently drop an identified runtime. Each row uses the same TV-remote launch → Xbox-controller play → exact return requirement with no mouse. SuperTux is a native launch fixture and satisfies none of these emulator rows.

For every advertised supported platform: prove fresh appliance/runtime provisioning and repair, remote-only game/firmware import as applicable, playable game launch, Xbox input, profile/save preservation, exact return, offline cold boot and upgrade rollback. One accepted emulator does not complete the whole declared platform matrix. Optional save-state controls are exposed only when verified; ordinary game saves must work without enabling them.

## 4. Ownership, identity and persistence

Use one profile-local games catalog and repository-supported SQLite facilities if available. Before coding, verify existing Qt SQL/build support; if a new dependency is required, resolve that scope decision first. Do not introduce a service or framework merely to provide indexing.

| Entity | Contract |
| --- | --- |
| Game | Stable opaque ID, title/sort title/aliases, summary, franchise/series, tags, completion preference and managed artwork references. |
| Edition | Stable ID linked to a Game; original platform, region/revision, provider identity, preferred launcher-profile reference. Never merge editions solely by title. |
| Installation | Edition reference, source reference, authorized relative file/provider target, availability and last verification; paths are locators, not identities. |
| Source | Profile-scoped authorized folder or adapter/account binding, supported formats, explicit scan policy and capability state. No credentials in layouts/exports. |
| Launcher profile | Validated runtime/application reference and typed supported options. No executable snippets, shell commands or arbitrary environment fields in layouts. |
| Activity | Last confirmed launch/session, duration only when measurable, provenance and confidence. Failed attempts are not play sessions; launching is not save-state resume. |
| Collection / shelf | Stable identity; manual ordered membership or bounded declarative filters/sort; presentation belongs to shared composition/appearance. |
| Artwork | Content-addressed managed asset, type/size validated, local provenance and title fallback. No raw external image URL dependency. |

Isolde owns manual organization and observed activity, not a second copy of storefront entitlements or emulator saves. Imported metadata records provenance; refresh never overwrites manual corrections. Display caches are disposable. A game appearing on several shelves references the same canonical record.

Catalog edits use database transactions, schema validation and explicit migration/backup/rollback tests. Document/artifact saves retain validate → temp → fsync → rename and rollback. Define recovery across catalog/artwork/export boundaries: stage assets first, commit references transactionally, and collect unused assets only after verified success. A crash must leave the previous or complete new state, never dangling references. Invalid layout reads fall back without rewriting originals; catalog migration is a separate explicit versioned operation.

Profile switch invalidates clients, queries, thumbnails and session callbacks by generation/token. External Steam login and machine-global game saves are not automatically isolated by a profile-local catalog. Advertise shared external state honestly; prove isolation before enabling distinct account/save claims. Switching away from an active game requires a deliberate close/recovery workflow.

## 5. Discovery and adapter capability gate

Before a Steam or emulator adapter is implemented, record: supported platform/version; documented local interface or authorized format; account selection; offline availability; stable IDs; installed/owned distinction; field provenance; launch route; session completion detection; required permissions; fixtures; and unsupported behavior. Fail the gate if safe ownership or reliable identity cannot be established. Preserve the ordinary Open Steam fallback.

Use independent capability flags for discovery, launch, install handoff, metadata, artwork, playtime and save-state operations. An adapter may launch manually registered games without supporting automatic discovery. No Steam Web API requirement in the offline baseline; optional network enrichment requires explicit opt-in and OFF must produce zero external requests.

Folder import is an explicit, cancelable background job over granted roots. Canonicalize and verify containment, including symlinks; reject escapes and unsupported files. Filter extensions without reading game contents by default. Preview uncertain matches, editions and multi-disc sets; approve before catalog mutation. Missing/removable sources mark installations unavailable and preserve metadata/tags. Relinking previews changes rather than deleting/reimporting identities. Discovery never moves, renames or modifies game files, saves or provider databases. Explicit installation is a separate scoped write workflow under §3.1.

Proposed initial bounds, to be ratified as schema constants with tests: 50 results/page; 256 characters/title; 4 KiB/summary; 64 sources/profile; 256 collections/profile; 64 rules/collection with depth at most 4; 50,000 catalog entries/profile; 10,000 candidates/import preview; 30-second adapter request deadline. Import limits produce an explicit partial-result/continue choice, never silent truncation. Cancel superseded queries; reject stale replies. Metadata/artwork jobs have bounded queues, byte budgets and concurrency. Reuse existing artwork limits. Existing layout limits remain 4 MiB, 1,024 components, 128 categories, 64 pages/profile and 4–16 × 3–10 grids; runtime catalog rows do not become thousands of persisted page components.

## 6. Shared UI and declarative contracts

Use PageSurface/ComposedHome and the existing page-library/provider pattern, Studio catalog, inspector, appearance model and centralized ActionDispatcher. Names such as GamesService/GamesShelves are provisional responsibilities, not a mandate to copy Kodi classes or build a parallel renderer. Add a new GameCard only if existing card presentation cannot express the required result.

A page stores shelf queries and stable game/edition references. The eventual versioned source/action shape must follow ComponentSchema's actual conventions; do not copy the original speculative JSON into production. Unknown versions/types/IDs and unavailable capabilities fail with `errorString()` and preserve prior state. Revalidate installation, grant, profile and runtime at dispatch, not only when a card was created.

Packaged defaults: Continue Playing, Favorites, Recently Added, populated platform shelves (including PSP when present), All Games. Empty optional rows hide; an empty whole page retains Add source, Add game, Settings/Edit Home and Back. Navigation, rows, filters, platform badges, art/crop, labels, density, focus, spacing, corners and opacity are all validated data editable through Studio. Do not silently replace existing users' Games layouts. Preset adoption is explicit and cancelable.

Details presents Play, edition/version choice, availability, input compatibility, provenance and organization controls. Missing art uses a readable title tile. Search covers local titles, aliases, franchises and tags. Browse All supports virtualized grid, platform/franchise/tag/installed/favorite/completed filters and title/added/last-played sorts. Collection rules use validated declarative predicates, never expressions or scripts. Remote and desktop edits produce identical data and atomic draft/cancel behavior.

Default Continue Playing means launch again; only label “Resume save state” when the specific adapter proves that capability. Local save ownership stays with the game/emulator.

## 7. Launch, controller and return lifecycle

Resolve game → edition → installation → approved launcher profile at execution. One playable edition can launch from Play immediately; multiple editions use a preferred edition or explicit chooser. Unavailable runtime/source produces Repair/Retry/Cancel with origin intact. Installation handoff is separate, explicit and capability-gated; no implicit downloads on Play.

Create a bounded immutable launch request with profile generation, source grant, stable IDs, request/session token and structured origin (page, shelf/query, game/edition, selection and scroll). The trusted adapter creates argv without shell expansion and without mutating the app registry. Reject path escape, malformed provider IDs, unknown options, stale grants, duplicate launches and cross-profile references.

Lifecycle: resolving → starting → active → returning → idle, with bounded failure/recovery transitions. Child launcher exit is not proof the actual game exited: Steam may use an existing process. The adapter must demonstrate how it identifies the owned game/session and distinguishes launcher failure, game start and game completion. Do not terminate unrelated Steam clients or games. If this cannot be proved, label direct-launch lifecycle unsupported and retain explicit return/fallback rather than fabricating successful playtime.

Inside Isolde, D-pad + Confirm + Back + Menu/Search completes every management workflow with visible focus and on-screen keyboard. Inside a game, normal Back/Menu belong to the game. Use T12's configurable Return to Isolde binding (Home by default), existing compositor/native-host lifecycle and bounded graceful shutdown/recovery. Do not promise ordinary Back can quit every emulator. A remote must still operate Isolde when a controller disconnects; test simultaneous devices, reconnects and remapping without installing a new global input daemon.

Restore the exact origin on natural exit, canceled launch, failure and Home return through SessionManager/ActionDispatcher. If the card vanished, restore the nearest surviving card in its shelf, then shelf heading, then page recovery/navigation. No Kodi playback-return dependency. Profile teardown must discard old callbacks rather than restore another profile's focus.

## 8. Delivery sequence and entry points

Each slice includes schema/data, remote UX, visible result, failure/retry, focus return and tests. Inspect only the listed entry areas and follow their immediate build/wiring references; file lists are starting points, not an artificial ban on necessary CMake/resource/test registration.

| Slice | Entry areas | Exit gate |
| --- | --- | --- |
| G0: capability/design spike | This plan; master plan §6; AppRegistry; SessionManager; NativeAppHost/NativeSandbox; current Steam evidence | Current primary-source adapter matrix and bounded guest probe evidence; runtime/storage/input ownership decisions; explicitly recorded unsupported fields. No product completion credit. |
| G1: one local game end to end | ComponentSchema, ActionDispatcher, profile runtime wiring, HomePages/PageData, existing card/library UI, SessionManager | Manually add one authorized local game; details → Play → real game → exact return offline; missing runtime/path/failure and cancel preserve origin/data. |
| G2: catalog and import | G1 model/store, source grant boundaries, remote Settings/keyboard, existing artwork manager | Preview/cancel/commit import, manual metadata/art, relink/unavailable source, duplicate/multi-disc review, backup/restore and deterministic write-failure recovery; game/save bytes unchanged. |
| G3: organization and Studio | Shared provider/shelf interfaces, CommandCenter, StudioCatalog/Inspector/LayoutEditor | All Games, filters/search, favorite/history, manual/smart collections and edition selection; shared appearance and remote/desktop draft parity; one record appears across shelves. |
| G4: Steam adapter | Approved G0 interface; registry/catalog/session boundaries | Actual authorized local discovery where supported, account/offline states, structured direct Play, real session ownership and exact return. Big Picture alone fails this gate. |
| G5: integrated emulator delivery | G0 platform/runtime matrix; existing package/provisioning, profile settings, source import and SessionManager boundaries | Provision pinned runtimes as part of the appliance; PSP initial acceptance plus every declared supported platform passes Isolde-only setup/import/configuration, TV-remote launch, Xbox gameplay, offline cold boot, profile/save preservation, repair/update rollback and exact return. No manual emulator installation or desktop-menu prerequisite. |
| G5a: local PC copies and installers | G0 runtime capability matrix; G1/G2 source/catalog and native session boundaries; approved installer/provisioning interfaces | Existing local folder launches without Steam; local installer preflight → consent → install → verify → play → exact return; failure/cancel/uninstall preserve pre-existing files and saves. |
| G5b: inserted PC discs | G5a; supported OS optical-device/mount interface; typed disc source and remote recovery UI | Real PC disc insertion → identify → install or play → exact return; disc-required launch, multi-disc swap, wrong/missing disc, eject/removal and retry verified on physical optical hardware. No autorun or assumed universal compatibility. |
| G6: scale and installed acceptance | Existing native/QML suites and VM oracle | 5 and 5,000-game fixtures, crash/recovery, two-profile isolation, disconnected cold boot and both standing visual recreation/reset pairs; physical controller acceptance separately evidenced. |

G5a requires G0/G1/G2; G5b requires G5a; G6 requires integrated-emulator acceptance for the declared platform matrix, both local-copy and physical-disc acceptance, and the other enabled adapters. G2 depends on G1; G3 on G2; G4/G5 require G0 and the common G1/G2 contracts. Do not postpone lifecycle feasibility until the end or wait for all providers before delivering organization. Commit one logical slice on `agent-backlog-progress` under its assigned real task ID; no G-number substitutes for backlog task IDs.

## 9. Verification and evidence requirements

Extend existing `tst_components`, native session/input suites and QML offscreen coverage first; add a dedicated games suite only where it creates a useful boundary. Fixtures simulate adapter results, not successful real gameplay.

| Area | Required cases / evidence |
| --- | --- |
| Contract/security | Unknown schema/action/options; malformed IDs; shell metacharacters; traversal/symlink escape; replaced file at launch; oversized documents/imports; revoked grants; no command execution from layout. |
| Catalog integrity | Repeated import idempotence; same title distinct editions; multi-disc grouping; manual corrections survive refresh; missing/relinked source; invalid migration; disk full/write failure/crash and backup restore; assets/catalog stay consistent. |
| Isolation/cancellation | Two profiles with conflicting IDs; switch during import/query/artwork/launch; late responses dropped; external account/save sharing explicitly tested or unsupported; no cross-profile data or focus. |
| Decisive couch acceptance | Complete §1.1 on each supported source route: launch using TV remote only → actual Xbox-controller gameplay → exact return; all intervening setup/dialogs/recovery mouse-free. Physical keyboard, desktop rescue or mouse dependency fails the gate. |
| UI/input | Empty/loading/partial/unavailable/error; offline keyboard search; details/edition chooser; invalid targets; remote/pointer byte parity; exact return after success/failure/Home; vanished-origin fallback; Settings always reachable. |
| Native lifecycle | Already-running Steam, bootstrap exits before game, duplicate launch, failed start, crash/hang, refusal to close, natural exit and Home; only owned processes affected. Real gameplay evidence distinct from sign-in/bootstrap. |
| Local copies / PC discs | Native and supported compatibility-runtime titles; installed folder and installer copy; real optical insertion/read/installation; hotplug and changed mount path; wrong/missing/multi-disc media; removal during install/play; eject busy state; space/permission/read failures; cancel/retry/uninstall preservation; no autorun. VM-mounted fixture/image proves logic only, not physical-drive support. |
| Offline/ownership | Disconnect and cold boot; local search/art/launch work; Web OFF zero Isolde external requests; no unexpected downloads; game/save/provider files unchanged by Isolde. External client traffic is measured separately and not hidden behind an Isolde-only assertion. |
| Scale/accessibility | Five-game clean defaults; 5,000-entry paging/virtualization; rapid scroll and search cancellation; bounded memory/queues; reduced motion, contrast, safe areas and TV-distance text; physical Xbox disconnect/reconnect and remote fallback. |

G0 must record target hardware and numeric performance budgets before scale implementation; proposed targets are first cached page within 1 second and local search within 300 ms after debounce on that target. Record measured startup/query/render latency, peak memory, cache size and dropped frames rather than claiming “fast enough.” VM input simulation cannot certify physical controller compatibility or target-hardware performance.

When implementation is authorized, read `docs/vm-oracle.md` and use only `tools/vm-test.ps1` Test/Visual/Deploy/Status/Cancel/Recover; reuse incremental cache, poll the same session and stop at the first named failure. Run `ctest --test-dir build --output-on-failure` through the bounded guest workflow plus selected native/QML journeys. Capture both `tests/visual/home-reference` and `home-pastel-reference` recreation/reset from the generic renderer. Preserve normal profile/layout hashes and useful evidence.

For each accepted slice retain source commit, tool/run IDs, test logs, real screenshots with reviewed verdicts, profile/input/network conditions, before/after file hashes, supported capability versions and open failures. Never turn a VM bootstrap, synthetic catalog or successful process spawn into a gameplay/physical-hardware pass.

## 10. Remaining decisions and planning status

Resolve during G0: supported first local runtime/fixture, repository SQLite availability, safe Steam local discovery surface, reliable game/session ownership, shared external account/save limitations, controller grant/remapping support, and target hardware budgets. Unsupported provider behavior must narrow the capability, not expand permissions or invent metadata.

Review result: retain the common Games page and shared Studio direction; replace speculative provider promises, the read-only-only catalog and backend-first rollout with the contracts and gated slices above. No checkbox credit or application implementation is claimed by this document.
