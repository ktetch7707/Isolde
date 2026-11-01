import xbmc
import xbmcvfs
import os
import queue
import time
import json
from bridge import LocalBridge
from playback import Playback
from artwork import Artwork
from playlists import Playlists

commands = queue.Queue(maxsize=128)
class Monitor(xbmc.Monitor):
    def onNotification(self, sender, method, data):
        if sender == "xbmc" and method == "Player.OnStop" and isinstance(data,str) and len(data)<=4096:
            try:
                value=json.loads(data)
                if isinstance(value,dict): commands.put_nowait(("ended",value,playback.generation))
            except (ValueError,TypeError,queue.Full): pass
        if sender.casefold() == "isolde" and method.casefold().startswith("other."):
            action = method[6:].casefold()
            if action in ("pause", "stop", "home", "forward", "backward"):
                try: commands.put_nowait(action)
                except queue.Full: pass

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
        # On X11 the service can start before Kodi finishes showing its window.
        if minimize_after is not None and time.monotonic() >= minimize_after:
            if playback.movie is None:
                xbmc.executebuiltin("Minimize")
            minimize_after = None
        try:
            while not commands.empty():
                command=commands.get_nowait()
                if isinstance(command,tuple):playback.stopped_notification(command[1],command[2])
                else:playback.control(command)
            bridge.poll()
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
