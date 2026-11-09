# Navigation

## T26 route correction — All Apps versus Search

[shell-surfaces-design.md](shell-surfaces-design.md) section 3 and T26 define a dedicated themed installed-app gallery. All Apps/More Apps and legacy `app-drawer` actions resolve to that destination; universal Search is a separate explicit route. Keep compatible reads without rewriting layouts. Preserve the full Home/page → gallery → app → gallery → origin focus/scroll chain. The HOME/SESSION prototype description below is historical, not a complete inventory of current overlays/pages or permission to route All Apps into Command Center. T28 places the Web switch in Search's header, outside results.

The current T26 coding slice adds typed `open-apps` and `open-search` actions, maps legacy `app-drawer` at dispatch time without rewriting stored layouts, and renders All Apps as its own remote-operated shell surface. The gallery projection uses registered launchables and installed catalog entries, with stable-ID selection, local-only filtering, state labels and return-to-origin handling. Its grid/list/title/density/art/label/filter defaults are packaged JSON; profile-persisted gallery overrides and Studio controls, the broader route audit and installed/visual acceptance remain open. This implementation note is not acceptance evidence.

The prototype has two explicit shell states: HOME and SESSION.

- HOME accepts directional and Confirm commands.
- SESSION treats Back as contextual exit to HOME.
- Home is global and returns directly to HOME from either state.

The home grid uses deterministic three-column navigation at TV resolution (two columns in a narrow development window). Movement stops at edges; it does not wrap. The selected app is stored by stable ID in `SessionManager`, then resolved back to a current grid index after exit. Config reordering therefore does not make focus restoration point to the wrong app.

Keyboard mappings are normalized by `InputManager`. A future evdev, Bluetooth, RF, or CEC input adapter should emit the same commands rather than manipulating QML focus directly.
