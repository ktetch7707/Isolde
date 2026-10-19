# Navigation

The prototype has two explicit shell states: HOME and SESSION.

- HOME accepts directional and Confirm commands.
- SESSION treats Back as contextual exit to HOME.
- Home is global and returns directly to HOME from either state.

The home grid uses deterministic three-column navigation at TV resolution (two columns in a narrow development window). Movement stops at edges; it does not wrap. The selected app is stored by stable ID in `SessionManager`, then resolved back to a current grid index after exit. Config reordering therefore does not make focus restoration point to the wrong app.

Keyboard mappings are normalized by `InputManager`. A future evdev, Bluetooth, RF, or CEC input adapter should emit the same commands rather than manipulating QML focus directly.
