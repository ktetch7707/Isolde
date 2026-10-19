# Home components, sources and actions

Implemented in 0.0.1.2, October 16, 2026. `ComponentSchema` is the shared data boundary; `ActionDispatcher` is the shell execution boundary. Neither a layout nor a widget may provide executable commands.

Later October 16 extension: optional `composition` data adds ordered tabs and one/two-lane section layouts. Category/item `presentation` maps select reusable banner, media, source and summary variants, independent geometry, local artwork crops and progress display. Managed artwork now also accepts JPEG. Contracts validate these fields; existing component/action/source identities and layout migration remain unchanged. See [home-composition.md](home-composition.md) for implemented fields and remaining editor/backend work. This supersedes the older blanket statement below that all tabs/composition are future work.

## Versioned data

Home document version 2 stores `components`, ordered `categories` and the existing `theme`. Each component, source and action uses `schemaVersion: 1`. The component's stable `id` identifies focus and editing independently of its app/collection/handler binding.

```json
{
  "schemaVersion": 2,
  "theme": {"backgroundStart": "#111A32", "backgroundEnd": "#080C18"},
  "categories": [{"id": "library", "name": "Library"}],
  "components": [{
    "schemaVersion": 1,
    "componentType": "launcher",
    "id": "family-films",
    "kind": "action",
    "name": "Family films",
    "categoryId": "library",
    "size": "wide",
    "source": {"schemaVersion": 1, "provider": "kodi", "kind": "collection", "id": "collection-id"},
    "action": {"schemaVersion": 1, "type": "open-collection", "collectionId": "collection-id"}
  }]
}
```

Supported component types are launcher, clock and collection media-shelf. Home documents currently persist launcher/clock placements; collection shelf placement continues to use the existing per-profile collection pin/order/count settings. `KodiShelves.components` exposes each generated shelf through the same component/source/action contract, with resolved child launchers in `items`. This deliberately preserves existing shelf configuration. Nested composition, arbitrary shelf placement, tabs and other widget families belong to the later editor slice.

Presentation fields remain compatible: size, category, symbol, accent, opacity, labels/logo options, roundness and managed PNG path. `kind`, `appId` and `widgetType` remain renderer/editor projections; they do not provide a second execution route. An app component's action and app ID must agree. A versioned source must match the action's provider, kind and identity; conflicting routes are rejected.

| Source provider/kind | Capability boundary |
| --- | --- |
| `apps/app` | Installed registry app; dispatch `launch-app`. |
| `handler/open-folder`, `open-file`, `open-disc`, `open-url` | Installed handler must declare the exact action type. Existing desktop action adapters remain responsible for argument construction and sandbox grants. |
| `kodi/collection` | Active profile's saved collection; dispatch `open-collection` or validated `play-movie`. Movie membership is checked against the refreshed collection page before playback. |
| `shell/clock` | Information component; no launch action. |
| `shell/settings`, `shell/desktop-mode` | Typed built-in route; desktop uses the declared Desktop app, never shell commands. |

Unknown versions/types, conflicting sources, unsafe identities, invalid sizes/opacity, non-managed saved artwork paths and invalid targets are rejected. Targets use absolute paths or HTTP(S) URLs without embedded credentials. The contracts do not grant filesystem access. Layout input is bounded to 4 MiB, 1,024 components and 128 categories. Collection queries continue to use the bounded local bridge, with no external search provider.

## Compatible loading and backups

Version-1 `items` documents are normalized in memory. Reading does not rewrite a user's layout. On the first successful edit, the original valid v1 bytes are saved in `home-layout.json.v1.backup.json` before the v2 document is atomically committed. An existing backup is not overwritten. If the backup cannot be written, the edit fails and the previous layout remains. The existing reset action returns to shipped defaults; it does not remove accounts, apps, Kodi data or media.

Invalid or unsupported documents fall back to the shipped defaults and retain the original on disk. Missing apps, missing handlers, unsupported handler capabilities and deleted collections are availability errors, not malformed layout documents: their components stay visible and editable. Uninstalling an app keeps user launchers; reinstalling resolves their stable binding again. Existing older releases do not read v2 documents; their compatible v1 layout is retained in the backup for explicit downgrade recovery. No automatic downgrade of newer edits is claimed.

## Remote interaction and recovery

Home's permanent Settings and Edit Home controls are outside the deletable layout. Up from the first visual row reaches them; Left/Right chooses a control, OK opens it, and Down returns to content. They remain available with zero items and do not require a working Home button. Menu/Search opens Command Center. A missing source tile opens Customize Home, where **Repair / change source** offers compatible installed apps or saved local collections. Back cancels. Applying a replacement preserves the component ID, position, category, target and appearance.

All Home app/action/source/shelf activations call `ActionDispatcher`. It revalidates input at execution, checks the current source/capability and session/frontend ownership, and dispatches through supported session or Kodi APIs. Library-opening signals tell QML which view to present; QML no longer interprets a Kodi-specific tile action. Legacy `kodi-collection` actions are normalized for compatibility. SessionManager also validates direct action calls so older callers cannot bypass the action allowlist.

The dispatcher retains the originating component ID across app sessions and managed playback. Home first restores that exact ID, supports legacy app-ID callers, and uses a surviving clamped selection if the origin disappeared. Playback/view closure retains Home's existing scroll state. Presentation changes to a collection retain its query client and results; explicit refresh remains a query operation.

## Verification

`component_contracts` covers migration/backup preservation, malformed and conflicting definitions, missing apps/handlers/collections, compatible rebinding, empty layouts, dispatch rejection, chosen handlers and exact origin identity. A deterministic write-failure case proves moving/rebinding cannot reload another layout or discard the active profile's in-memory state. `home_component_navigation` exercises the real Home and Customize QML with remote commands, including cancellation, repair, same-handler distinct tiles, reordering, deleted-origin fallback and empty-Home recovery; all branding/source-header combinations stay within the available content area.

For a real graphical VM round trip, run `python3 tools/home-components-acceptance.py /path/to/bin/isolde`. It uses a disposable config/data/cache root, X11 remote keys and a temporary pause of the normal service with guaranteed restoration. It checks repair/cancel, v2 persistence, original backup, last-launcher removal and rebuilding an empty Home through the permanent editor control. Evidence and logs stay in its printed `/tmp/isolde-components-acceptance-*` directory. This is UI acceptance, not capture/CEC or real-media qualification.

Final deployed verification (October 16): the build-and-test installer passed all 12 suites and activated release `45ca0a45d4f8e43f15e53b4d` in Isolde-Test. The same graphical acceptance passed against that installed release; evidence is `/tmp/isolde-components-acceptance-bj0lpgwz/result.json`, with installer output at `/tmp/isolde-order-vsDgGY/components-install.log`. The normal Household UI was restored and visually inspected, with `ActiveState=active` and `NRestarts=0`. Normal user layouts were read without migration writes; acceptance edits used the disposable profile only.
