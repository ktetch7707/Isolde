# Gamepad topology during native game sessions

Native game sessions now retain their launch-time, gamepad-only device grant and
the native session host watches Linux input sysfs for bounded gamepad endpoint
changes. A small status protocol reports connected endpoints, endpoints granted
at launch, newly attached endpoints, and launch-granted endpoints that vanished.
The shell displays a non-focus-stealing notice that explains the required
recovery action.

This is hotplug detection and recovery guidance, not live device injection. The
current bubblewrap mount namespace cannot add an individually filtered device
node after launch. Newly attached devices therefore remain unavailable to the
running game; save when possible, return to Games, and relaunch to take a new
least-privilege device snapshot. Exposing the entire host input directory or
granting broad mount/device capabilities was rejected because it would also
expose keyboard and mouse devices.

The status protocol contains only bounded counts, is scoped to the active native
game session, and is cleared on session teardown. Its topology tests use
simulated endpoint sets; no physical controller or installed-game acceptance is
claimed.
