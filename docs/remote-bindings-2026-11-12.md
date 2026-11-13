# Controller-style Remote Setup — T12.3

User direction: Settings should list remote actions like a game's controller
settings, and let the user press a remote button to bind each action.

The Remote Setup page now lists Navigation (Up/Down/Left/Right/Select/Back),
Shortcuts (Home/Menu/Search), Playback, and Volume and power. The packaged
`config/remote-buttons.json` declares the action inventory and category labels.
The page uses the shared surface appearance and scrolls the selected row into
view. Select starts capture; the raw press is consumed before ordinary shell
actions, including Home and Power. Review offers Save binding, Choose another
button and Cancel. Menu offers confirmed removal of one custom binding; Restore
all default bindings has a cancel-first confirmation. The selected row survives
review/cancel/save. Capture expires after20 seconds; review expires after30.
Back cancels listening directly except when assigning Back itself, where the
first Back press captures the button and a second cancels review. Escape,
leaving the page or application deactivation also cancels without writing.

The new binding is an additional key for an existing semantic action. Existing
standard keyboard/media keys remain available for recovery. Assigning a key
already used by another action is refused with its current action named; no
silent replacement occurs. A remote that needs reassignment of an already used
standard key is outside this additive mapping support matrix. Modifier-only
keys, Ctrl/Alt/Meta chords, unknown keys and auto-repeat cannot become bindings.

`ProfileRuntime` supplies the active profile's `remote-bindings.json` path.
The bounded version1 document holds at most14 command/key pairs. Loading rejects
unknown versions/commands, fractional keys, duplicates, standard-key conflicts,
oversize files and linked paths, preserving stored bytes and using defaults.
Saves use QSaveFile, retain previous bytes in an atomic `.bak`, and publish the
in-memory mapping only after commit. Failure keeps the candidate available for
retry and preserves the old live mapping. Profile teardown removes the capture
filter and timer with the runtime.

Supported: key/media-key events actually delivered to Isolde by compatible
keyboard/HID USB, Bluetooth or receiver devices. This is not new Bluetooth
pairing, device-specific identity/scan-code discovery, raw IR/CEC reception,
gamepad-axis support or physical device qualification. Custom mappings apply to
Isolde screens; native/external apps and Kodi keep their existing return/input
controls. Full T12.3 installed/external-window/offline/hang/retry acceptance stays
open. No installed release is changed by these edits or focused checks.

## Verification

- `remote-bindings-20261112-a`: host sandbox denied the runner's loopback SSH
  connection before any guest test. Retried with the authorized VM runner's
  network access.
- `remote-bindings-20261112-b`: build succeeds; focused tests expose a new bug:
  the modifier-key numeric range also included function keys, rejecting capture.
  Fixed with an explicit modifier-key set; Escape is reserved for cancellation.
- `remote-bindings-20261112-c`: refused before build because another owned run,
  `round2-20261112-web-controls-s`, owns the VM. Status confirms a healthy
  Store test build/CTest run. It was left undisturbed; verification awaits its
  lock release.
- `remote-bindings-20261112-d`: focused VM build and registration pass; all four
  behavioral test methods pass (six QtTest results including setup/teardown),
  zero failures/skips. Reviewed all three `studio-test-remote-*.png` captures:
  selected Navigation row, clear save/cancel review and scrolled Recovery row.
  Evidence: `build/vm-oracle/remote-bindings-20261112-d/report/LastTest.log`,
  `ctest.xml` and `test-result.json`. This is offscreen renderer/input evidence.
- `remote-bindings-20261112-e`: the additional actual application-build check
  was refused while the existing Store owner's installed `visual-store-web`
  journey holds the VM. It was left undisturbed.
- `remote-bindings-20261112-f`: actual `isolde` executable/QML-cache build and
  all focused cases pass. Reviewed its three renderer captures as well.
- Review found and repaired a second usability defect: listening originally
  required Escape or timeout to cancel. Ordinary remote Back now cancels
  immediately, with the explicit two-press path for assigning Back itself.
  `remote-bindings-20261112-g` compiles the app and passes the real UI/atomic/
  isolation cases, but exposes a stale test assumption: the duplicate fixture
  used a custom Back button, which now correctly cancels listening. The conflict
  fixture is corrected to use Down instead. Its three captures were reviewed.
  `remote-bindings-20261112-h` passes the actual application build and all four
  behavioral cases (six QtTest results), zero failures/skips, including that
  correction, direct remote Back cancellation and application-deactivation
  coverage. All three final renderer captures reviewed. Final evidence is in
  `build/vm-oracle/remote-bindings-20261112-h/report/LastTest.log`, `ctest.xml`,
  `test-result.json` and `studio-test-remote-{actions,review,recovery}.png`.

No full application CTest/standing-reference rerun or installed acceptance is
claimed. This user-directed input slice does not close T12; its broader task
closure retains those gates. T12.3's original half-earned group is split with
explicit weights: the previously earned half is preserved, this narrow
native/QML milestone earns one eighth, installed/external remapping retains one
eighth, and all other remaining requirements retain one quarter. T12 is52%;
project accounting also preserves the independent concurrent Store evidence.

## Rear physical keyboard / accept keys follow-up

User clarification: the remote has a physical keyboard on its rear. During text
entry, its keys must be accepted as text rather than remote shortcuts.

The shared OnScreenKeyboard now registers a visible/enabled text target with
the profile's InputManager. With Accept Keyboard Keys enabled (the default),
delivered printable key text is consumed before semantic remote mapping. Thus
H/M, spaces, plus/equal and even a custom letter navigation binding type their
actual characters while a keyboard is open. Shift and delivered Unicode text
are preserved; Backspace deletes a Unicode code point without splitting a
surrogate pair. Both physical and onscreen appends obey the keyboard's4096-unit
default maximum and refuse overflow visibly. Password values remain masked.
Input text is neither logged nor persisted in the bindings document.

After physical typing, Enter accepts the text. A D-pad arrow returns to onscreen
selection, where Confirm selects the highlighted key as before. Back/Escape
keeps the existing cancel route. Consumed releases and held Enter repeats cannot
click through to the screen revealed after text acceptance. Once no text
keyboard is visible, ordinary remote mappings resume. Ctrl/Alt/Meta chords
cannot fall through into global letter shortcuts while text entry is active;
delivered GroupSwitch text is allowed. Clipboard shortcuts and composed IME
sessions are not added by this key-event implementation.

Every shared keyboard includes a remote-operable Accept keys action, displayed
as Keys: Text / Keys: Controls. Settings → Remotes also has Accept Keyboard Keys.
Controls mode preserves letter-emitting remotes' navigation bindings inside
text entry. The preference is profile-local and atomic, using an optional
boolean `acceptTextKeys` in the existing version1 document. Older documents
default to true without being rewritten; malformed booleans are refused. A
failed preference save preserves both stored and active mode and surfaces retry.
Keyboard target registration is removed on hide/destruction, with guarded
references preventing profile teardown from retaining a client.

Verification: `remote-keyboard-20261112-a` stops at oracle self-test: four checks
load an acceptance helper whose parser assumes the old keyboard actions list.
Both Guide/Home helper parsers now derive the appended Accept keys action, and
the exact-layout/D-pad-only regression is updated. No application pass was
claimed from that failed preflight.

`remote-keyboard-20261112-b` passes all120 runner self-tests, the actual
application build and both focused registrations: studio_shell17 QtTest results
and remote_bindings8, zero failures/skips. All22 renderer captures reviewed,
including masked physical typing, selected text/control toggle, short-panel
Done, three keyboard contrast palettes and unchanged Network/Store surfaces.
Three AudioSettings Connections warnings are from the existing isolated audio
double lacking `changed`; they are recorded without claiming a new audio fix.
Evidence: `build/vm-oracle/remote-keyboard-20261112-b/report/LastTest.log`,
`ctest.xml`, `oracle-self-test.log` and `studio-test-*.png`.

The focused suite now
builds the application and runs both remote_bindings and studio_shell, covering
the shared short-panel keyboard, Network/Store and theme/focus behavior as well
as physical text precedence, Unicode deletion, mode toggle, repeated Enter,
cancel, preference compatibility/isolation and deterministic rollback/retry.
No deployment, full-suite rerun or broad T12 closure is claimed. T12.2 retains
its previously earned half; the physical-input milestone earns one eighth and
the other installed requirements retain three eighths. Original group/task
weights are unchanged. T12 is54%; project acceptance remains86%.

`remote_bindings` exercises capture/review/save/reload, no-write cancel,
standard/custom conflicts, repeat/chord/invalid input, profile isolation and
teardown, malformed-file preservation, backup and deterministic write-failure
rollback/retry. Its real QQuickWindow test drives the page with actual key
events, including capture, review cancellation, save, Menu restore and scroll
to the all-defaults confirmation. UI captures are renderer evidence, not an
installed-window or physical-remote claim.

## Pending combined repair

Supported function-key and ASCII bindings for navigation, Confirm, Back, Home,
Menu and Search now also enter the external Chrome bridge through its existing
private options payload. The existing Chrome boundary journey uses custom
Down/Confirm/Menu/Back and retains standard history/keyboard paths. This source
change has not yet run in the VM. Native application/raw-device mapping and the
remaining installed matrix remain open; do not infer general external support.
