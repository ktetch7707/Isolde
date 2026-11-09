# Gamepad topology during native game sessions

Native game sessions retain their launch-time, gamepad-only device grant. The
native session host watches Linux input sysfs for bounded gamepad endpoint
changes and creates only verified evdev character nodes (input major, expected
event/js minor, and gamepad key capabilities) inside the private `/dev/input`.
The helper receives only `CAP_MKNOD` in its user namespace; the game child gets
no capabilities and `no_new_privs`. Detached helper-created nodes are removed
only when their device number still matches. Keyboard and mouse nodes are never
created or granted.

A bounded status protocol reports connected endpoints, launch-granted
endpoints, newly attached endpoints, and launch-granted endpoints that
vanished. The shell displays a non-focus-stealing notice. A newly added node
does not guarantee that the game or runtime rescans devices; unsupported
hotplug behavior still calls for saving, returning to Games and relaunching.

The host input directory is never bind-mounted and no mount capability is
granted. The status protocol contains only bounded counts, is scoped to the
active native game session, and is cleared on session teardown. Tests use
simulated endpoint sets and device-number fixtures; they do not prove kernel
user-namespace device-node creation or runtime/game hotplug behavior. No
physical-controller or installed-game acceptance is claimed.
