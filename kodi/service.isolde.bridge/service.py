import xbmc
import xbmcvfs
import os
import queue
import time
from bridge import LocalBridge
from playback import Playback
from artwork import Artwork
from playlists import Playlists

commands = queue.SimpleQueue()
class Monitor(xbmc.Monitor):
    def onNotification(self, sender, method, data):
        if sender.casefold() == "isolde" and method.casefold().startswith("other."):
            action = method[6:].casefold()
            if action in ("pause", "stop", "home", "forward", "backward"):
                commands.put(action)

monitor = Monitor()
playback = Playback(xbmc.executeJSONRPC, xbmc.executebuiltin, os.environ.get("ISOLDE_MANAGED_KODI") == "1")
bridge = LocalBridge(xbmcvfs.translatePath("special://profile/isolde"), xbmc.executeJSONRPC, playback,
                     Artwork(xbmc.executeJSONRPC, xbmcvfs.translatePath("special://profile/Thumbnails")),
                     Playlists(xbmcvfs.translatePath("special://profile/"),playback.managed))
try:
    bridge.start()
    if os.environ.get("ISOLDE_MANAGED_KODI") == "1":
        # Supported window action; no injected keystrokes or database writes.
        xbmc.executebuiltin("Minimize")
    xbmc.log("Isolde: private local bridge ready", xbmc.LOGINFO)
    minimize_after = time.monotonic() + 3 if playback.managed else None
    while not monitor.abortRequested():
        bridge.poll()
        # On X11 the service can start before Kodi finishes showing its window.
        if minimize_after is not None and time.monotonic() >= minimize_after:
            if playback.movie is None:
                xbmc.executebuiltin("Minimize")
            minimize_after = None
        try:
            while not commands.empty():
                playback.control(commands.get_nowait())
            if playback.movie is not None:
                playback.status()
        except Exception:
            xbmc.log("Isolde: playback state unavailable", xbmc.LOGWARNING)
        # Pump Kodi's Python callback queue; socket polling alone does not
        # reliably deliver Monitor notifications from remote key bindings.
        if monitor.waitForAbort(0.01):
            break
except Exception:
    xbmc.log("Isolde: local bridge unavailable; no network fallback was enabled", xbmc.LOGERROR)
finally:
    bridge.close()
