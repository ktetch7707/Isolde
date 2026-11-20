# Fixture helper, executed only by Kodi's RunScript in a NEW Studio Demo profile.
# MEDIA_ROOT is prepended by seed-studio-demo.py; no bridge methods are expanded.
import datetime
import json
from pathlib import Path
import time
import xbmc
import xbmcgui
import xbmcvfs

root=Path(MEDIA_ROOT)
result={"ok":False}
try:
    userdata=Path(xbmcvfs.translatePath("special://profile"))
    marker=userdata/"studio-demo-complete.json"
    if marker.exists(): raise RuntimeError("Already seeded; refusing to reset viewing history")
    if root.parent.name!="Isolde-Studio-Demo" or json.loads((userdata/"studio-demo-scope.json").read_text())!={"profile":root.name,"media":str(root)}:
        raise RuntimeError("Not the newly generated demo profile")
    def rpc(method,params=None):
        reply=json.loads(xbmc.executeJSONRPC(json.dumps({"jsonrpc":"2.0","id":1,"method":method,"params":params or {}})))
        if "error" in reply: raise RuntimeError(str(reply["error"]))
        return reply["result"]
    if rpc("VideoLibrary.GetMovies").get("movies") or rpc("VideoLibrary.GetTVShows").get("tvshows"):
        raise RuntimeError("Native import requires an empty demo library")
    xbmc.executebuiltin("ActivateWindow(mediasettings)"); xbmc.sleep(700)
    for _ in range(4):
        if "Expert" in xbmc.getInfoLabel("Control.GetLabel(20)"): break
        xbmc.executebuiltin("Action(settingslevelchange)"); xbmc.sleep(300)
    xbmc.executebuiltin("ActivateWindow(home)"); xbmc.sleep(300)
    xbmc.executebuiltin("ActivateWindow(mediasettings,videolibrary.import)"); xbmc.sleep(500)
    for control in range(30,3000):
        if xbmc.getInfoLabel("Control.GetLabel(%s)"%control).strip().lower()=="import library":
            xbmc.executebuiltin("SetFocus(%s)"%control); xbmc.sleep(250); break
    xbmc.executebuiltin("Action(select)"); xbmc.sleep(700)
    dialog=xbmcgui.getCurrentWindowDialogId()
    xbmc.executebuiltin("SetFocus(450)"); xbmc.executebuiltin("Action(down)"); xbmc.executebuiltin("Action(select)"); xbmc.sleep(500)
    if str(root/"import") not in xbmc.getInfoLabel("Control.GetLabel(412)"): raise RuntimeError("Unexpected import folder; not confirming")
    xbmc.executebuiltin("SendClick(%s,413)"%dialog)
    deadline=time.monotonic()+30
    movies=[]
    while time.monotonic()<deadline:
        movies=rpc("VideoLibrary.GetMovies",{"properties":["title","file"],"limits":{"start":0,"end":50}}).get("movies",[])
        shows=rpc("VideoLibrary.GetTVShows",{"properties":["title","episode"],"limits":{"start":0,"end":50}}).get("tvshows",[])
        if len(movies)==8 and len(shows)==2 and sum(s.get("episode",0) for s in shows)==6: break
        xbmc.sleep(300)
    if len(movies)!=8 or len(shows)!=2: raise RuntimeError("Native import counts differ")
    expected={m["file"]:m for m in json.loads((root/"expected.json").read_text())}
    now=datetime.datetime.now()
    for index,movie in enumerate(movies):
        fixture=expected.get(movie["file"])
        if fixture is None: raise RuntimeError("Movie outside fixture; refusing history write")
        state=fixture["state"]
        params={"movieid":movie["movieid"],"playcount":1 if state=="watched" else 0,
                "resume":{"position":25+index*10 if state=="continue" else 0,"total":120},
                "art":{"poster":fixture["poster"],"thumb":fixture["art"],"fanart":fixture["art"],"landscape":fixture["art"]},
                "dateadded":(now-datetime.timedelta(hours=index)).strftime("%Y-%m-%d %H:%M:%S")}
        if state!="unwatched": params["lastplayed"]=(now-datetime.timedelta(minutes=index*17)).strftime("%Y-%m-%d %H:%M:%S")
        rpc("VideoLibrary.SetMovieDetails",params)
    for show in shows:
        folder=root/show["label"]
        if not folder.is_dir(): raise RuntimeError("Show outside fixture")
        rpc("VideoLibrary.SetTVShowDetails",{"tvshowid":show["tvshowid"],"art":{"poster":str(folder/"poster.png"),"fanart":str(folder/"landscape.png")}})
        episodes=rpc("VideoLibrary.GetEpisodes",{"tvshowid":show["tvshowid"],"properties":["file","episode"]}).get("episodes",[])
        for episode in episodes:
            if Path(episode["file"]).parent!=folder: raise RuntimeError("Episode outside fixture")
            number=episode["episode"]
            rpc("VideoLibrary.SetEpisodeDetails",{"episodeid":episode["episodeid"],"playcount":1 if number==1 else 0,
                "resume":{"position":45 if number==2 else 0,"total":120},
                "lastplayed":(now-datetime.timedelta(hours=number)).strftime("%Y-%m-%d %H:%M:%S"),"art":{"thumb":str(folder/"landscape.png")}})
    marker.write_text(json.dumps({"media":str(root),"seeded":now.isoformat()}))
    result={"ok":True,"movies":len(movies),"shows":len(shows)}
    xbmc.executebuiltin("ActivateWindow(home)"); xbmc.executebuiltin("Minimize")
except Exception as error:
    result["error"]=str(error)
finally:
    # Granted media is intentionally read-only inside Kodi. Evidence belongs to
    # this fresh profile's writable userdata, not beside the media files.
    (Path(xbmcvfs.translatePath("special://profile"))/"studio-demo-import-result.json").write_text(json.dumps(result,indent=2))
