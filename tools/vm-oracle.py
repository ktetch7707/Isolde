#!/usr/bin/env python3
"""Bounded Linux runner: tests, explicit guest provisioning and isolated acceptance."""
import argparse
import contextlib
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import selectors
import shutil
import signal
import socket
import stat
import subprocess
import tempfile
import sys
import tarfile
import time
import xml.etree.ElementTree as ET
from urllib.parse import urlsplit


@contextlib.contextmanager
def release_link_transaction(installer, prefix):
    """Caller holds the install lock; failed activation preserves both links."""
    original = {name: installer.checked_release(prefix, name) for name in ("current", "previous")}
    try:
        yield
    except BaseException:
        for name, target in original.items():
            installer.checked_release(prefix, name)
            if target is None:
                link = prefix / name
                if link.is_symlink(): link.unlink()
            else:
                installer.set_link(prefix / name, target)
        raise


def guest_disk_usage_diagnostic(locations):
    """Optional inventory; mandatory free-space and cleanup checks remain separate."""
    try:
        result = subprocess.run(["du", "-x", "-B1", "--max-depth=2",
            *[str(p) for p in locations if p.is_dir()]], capture_output=True, text=True, timeout=30)
        return {"usage": result.stdout[:65536], "errors": result.stderr[:2048], "exit": result.returncode}
    except subprocess.TimeoutExpired:
        return {"usage": "", "errors": "Optional disk inventory timed out after 30 seconds", "timedOut": True}


def remove_guest_index_cache(cache, home, mounts):
    """Only the guest's reproducible desktop index; never profile/application data."""
    cache,home=Path(cache),Path(home)
    if home.resolve()!=home.absolute() or cache!=home/'.cache/tracker3' or cache.resolve()!=cache.absolute() or cache.is_symlink():raise RuntimeError('Unsafe guest index cache path')
    if not cache.exists():return 0
    if not cache.is_dir() or cache.stat().st_uid!=os.getuid():raise RuntimeError('Unsafe guest index cache owner')
    if any(Path(m)!=Path('/') and (Path(m)==cache or Path(m).is_relative_to(cache) or cache.is_relative_to(Path(m))) for m in mounts):raise RuntimeError('Mounted guest index cache')
    total=0
    for directory,names,files in os.walk(cache,followlinks=False):
        for name in names+files:
            entry=Path(directory)/name;info=entry.lstat()
            if entry.is_symlink() or info.st_uid!=os.getuid() or not (stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode)):raise RuntimeError('Unsafe guest index cache entry')
            if stat.S_ISREG(info.st_mode):total+=info.st_size
    shutil.rmtree(cache)
    return total


def repository_dns_probe(host):
    if not isinstance(host, str) or not re.fullmatch(r"[A-Za-z0-9.-]{1,253}", host):
        raise ValueError("Invalid Android repository hostname")
    try:
        result = subprocess.run(
            ["getent", "ahosts", host], capture_output=True, text=True, timeout=8
        )
    except subprocess.TimeoutExpired:
        return {"host": host, "resolved": False, "error": "DNS lookup timed out"}
    addresses = sorted({line.split()[0] for line in result.stdout.splitlines() if line.split()})
    return {
        "host": host,
        "resolved": result.returncode == 0 and bool(addresses),
        "addresses": addresses,
        "error": result.stderr[:2048] if result.returncode else "",
    }


def resolver_status():
    command = shutil.which("resolvectl")
    if not command:
        return {"available": False, "error": "resolvectl is not installed"}
    try:
        result = subprocess.run(
            [command, "status", "--no-pager"], capture_output=True, text=True, timeout=8
        )
    except subprocess.TimeoutExpired:
        return {"available": True, "error": "resolvectl status timed out"}
    return {
        "available": True,
        "exit": result.returncode,
        "output": result.stdout[:16384],
        "error": result.stderr[:2048],
    }


def guide_backend_probe(home):
    """Read only the active Isolde profile's bridge, with no scan/tune/auth.

    Missing configuration is a capability finding, not a fabricated lineup.
    Never inspect another profile to find a more convenient working backend.
    """
    home=Path(home)
    result={"available":False,"playbackVerified":False,"profileIsolation":"active-only"}
    try:
        profiles=home/".config/Isolde/profiles.json"
        if any(path.is_symlink() for path in (profiles,*profiles.parents)):
            raise ValueError("Refusing redirected profile configuration")
        if profiles.is_symlink() or not profiles.is_file() or profiles.stat().st_size>65536:
            result["reason"]="Active profile configuration unavailable";return result
        active=json.loads(profiles.read_text())["active"]
        if not isinstance(active,str) or not re.fullmatch(r"default|[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}",active):
            raise ValueError("Invalid active profile")
        directory=home/".local/share/Isolde/native-profiles"/active/"kodi/home/.kodi/userdata/isolde"
        for path in (profiles,*profiles.parents,directory,*directory.parents):
            if path.is_symlink():raise ValueError("Refusing redirected profile path")
        result["activeProfile"]=active
        # Linux AF_UNIX paths are bounded; the descriptor path also prevents
        # a final directory symlink from redirecting the connection.
        descriptor=os.open(directory,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC)
        try:
            def query(method,params):
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as stream:
                    deadline=time.monotonic()+3
                    stream.settimeout(3)
                    stream.connect("/proc/self/fd/"+str(descriptor)+"/rpc.sock")
                    stream.sendall(json.dumps({"jsonrpc":"2.0","id":1,"method":method,"params":params}).encode()+b"\n")
                    data=b""
                    while not data.endswith(b"\n"):
                        remaining=deadline-time.monotonic()
                        if remaining<=0:raise TimeoutError("Guide probe deadline")
                        stream.settimeout(remaining)
                        block=stream.recv(min(65536,4*1024*1024+1-len(data)))
                        if not block or len(data)+len(block)>4*1024*1024:raise ValueError("Invalid guide response bounds")
                        data+=block
                    reply=json.loads(data)
                    if not isinstance(reply,dict) or reply.get("jsonrpc")!="2.0" or reply.get("id")!=1:raise ValueError("Invalid bridge response")
                    return reply
            version=query("JSONRPC.Version",{})
            if not isinstance(version.get("result"),dict):raise ValueError("Invalid Kodi version response")
            result["version"]=version["result"].get("version")
            channels=query("PVR.GetChannels",{"channelgroupid":"alltv","properties":["channelnumber","broadcastnow","broadcastnext","locked","hidden"],"limits":{"start":0,"end":50}})
            if "error" in channels:
                result["reason"]="PVR discovery unavailable through the configured bridge"
                result["bridgeErrorCode"]=channels["error"].get("code")
            else:
                catalog=channels.get("result",{})
                if not isinstance(catalog,dict):raise ValueError("Invalid channel catalog")
                rows=catalog.get("channels");limits=catalog.get("limits",{})
                if (not isinstance(rows,list) or len(rows)>50 or not all(isinstance(row,dict) for row in rows)
                    or not isinstance(limits,dict) or type(limits.get("total")) is not int or limits["total"]<len(rows)
                    or limits.get("start")!=0 or limits.get("end")!=len(rows) or (not rows and limits["total"]!=0)):
                    raise ValueError("Invalid channel page")
                result.update(available=True,totalChannels=limits["total"],sampledChannels=len(rows),
                    channelsWithNow=sum(bool(row.get("broadcastnow")) for row in rows),
                    channelsWithNext=sum(bool(row.get("broadcastnext")) for row in rows))
        finally:os.close(descriptor)
    except (OSError,ValueError,KeyError,TypeError):
        result["reason"]="Active profile's local Kodi bridge is unavailable or incompatible"
    return result

REPAIR_TESTS = {"studio_shell","games_remote_ui","component_contracts","home_component_navigation","studio_navigation","studio_behavior","audio_backend","games_provisioning","steam_games","optical_exec_adapter","native_profile_isolation","isolde_tests"}
RUNTIME_TESTS = {"games_provisioning","steam_games","portable_game_installer","optical_exec_adapter","native_profile_isolation"}
SUITES = {"all": None, "remote_bindings":("isolde_remote_binding_tests","isolde_studio_quick_tests","isolde"), "runtime_contracts":"isolde_native_sandbox_tests", "campaign_repairs": ("isolde_studio_quick_tests","isolde_games_ui_tests","isolde_component_tests","isolde_home_navigation_tests","isolde_audio_tests","isolde_native_sandbox_tests","isolde_tests"), "component_contracts": "isolde_component_tests",
          "sources": ("isolde_tests", "isolde_component_tests", "isolde_studio_quick_tests", "isolde_home_navigation_tests"),
          "guide_contracts": "isolde_guide_tests",
          "guide_remote_ui": "isolde_guide_ui_tests",
          "guide": ("isolde_guide_tests", "isolde_guide_ui_tests", "isolde_kodi_media_tests"),
          "games": ("isolde_component_tests", "isolde_games_ui_tests", "isolde_tests"),
          "store": ("isolde_tests", "isolde_studio_quick_tests", "isolde_web_remote_tests", "isolde_native_smoke"),
          "web_remote": ("isolde_web_remote_tests", "isolde_native_smoke"),
          "store_remote_ui": "isolde_studio_quick_tests",
          "media_controls": ("isolde_home_navigation_tests","isolde_kodi_media_tests","isolde_native_sandbox_tests","isolde"),
          "audio_backend": "isolde_audio_tests",
          "profile_browser_isolation": "isolde_profile_browser_tests",
          "home_component_navigation": "isolde_home_navigation_tests",
          "studio_navigation": "isolde_home_navigation_tests",
          "kodi_media_client": ("isolde_kodi_media_tests","isolde"),
          "desktop_mode": "isolde_desktop_mode_tests",
          "vpn": "isolde_vpn_tests",
          "studio_quick": "isolde_studio_quick_tests",
          "studio_safety": "isolde_studio_quick_tests",
          "studio_selection": "isolde_studio_quick_tests",
          "studio_artwork": "isolde_studio_quick_tests",
          "shell_appearance": ("isolde_studio_quick_tests", "isolde_home_navigation_tests", "isolde_surface_appearance_tests"),
          "studio_visuals": ("isolde_studio_quick_tests", "isolde_home_navigation_tests"),
          "app_icons": ("isolde_tests", "isolde_component_tests", "isolde_studio_quick_tests", "isolde_home_navigation_tests"),
          "studio_colors": "isolde_studio_quick_tests",
          "studio_workflows": "isolde_studio_quick_tests",
          "studio_cutover": "isolde_studio_quick_tests",
          "studio_design_contracts": ("isolde_studio_design_tests",),
          "studio_reuse": ("isolde_studio_quick_tests", "isolde_studio_design_tests"),
          "studio_remaining": ("isolde_studio_quick_tests", "isolde_studio_design_tests", "isolde_component_tests"),
          "studio_performance": ("isolde_studio_quick_tests", "isolde_studio_design_tests", "isolde_component_tests"),
          "studio_closure_regressions": ("isolde_studio_quick_tests", "isolde_studio_design_tests", "isolde_home_navigation_tests"),
          "studio_designs": ("isolde_studio_quick_tests", "isolde_studio_design_tests", "isolde_component_tests"),
          "studio_gestures": "isolde_studio_quick_tests",
          "studio_grid": "isolde_studio_quick_tests",
          "studio_destinations": "isolde_studio_quick_tests",
          "studio_ordered": "isolde_studio_quick_tests",
          "studio_policies": "isolde_studio_quick_tests",
          "studio_overlays": "isolde_studio_quick_tests",
          "studio_unit": "isolde_component_tests",
          "studio_behavior": "isolde_home_navigation_tests",
          "studio": ("isolde_studio_quick_tests", "isolde_component_tests", "isolde_home_navigation_tests", "isolde_studio_design_tests")}


def suite_selection(suite):
    """One allowlisted selection authority for build targets and CTest names."""
    targets = SUITES[suite]
    targets = list(targets) if isinstance(targets, tuple) else [targets] if targets else []
    pattern = "^(studio_quick|studio_safety|studio_selection|studio_artwork|studio_colors|studio_color_interactions|studio_workflows|studio_cutover|studio_performance|studio_reuse|studio_designs|studio_design_contracts|studio_gestures|studio_grid|studio_grid_controls|studio_destinations|studio_ordered|studio_policies|studio_overlays|studio_unit|studio_behavior)$" if suite == "studio" else "^" + re.escape(suite) + "$"
    if suite == "studio_closure_regressions":
        pattern="^(studio_quick|studio_colors|studio_color_interactions|home_component_navigation|studio_design_contracts|studio_performance|studio_cutover)$"
    if suite == "remote_bindings":
        pattern = "^(remote_bindings|studio_shell)$"
    if suite == "shell_appearance":
        pattern = "^(studio_selection|studio_shell|surface_appearance_contracts|home_component_navigation)$"
    if suite == "studio_visuals":
        pattern = "^(studio_quick|studio_artwork|studio_shell|home_component_navigation|studio_navigation)$"
    if suite == "app_icons":
        pattern = "^(isolde_tests|component_contracts|studio_artwork|studio_selection|home_component_navigation|studio_navigation)$"
    if suite == "studio_remaining":
        pattern="^(studio_quick|studio_performance|studio_design_contracts|component_contracts)$"
    if suite == "studio_colors":
        pattern="^(studio_colors|studio_color_interactions)$"
    if suite == "studio_performance":
        pattern = "^(studio_performance|studio_design_contracts|component_contracts)$"
    if suite == "studio_reuse":
        pattern = "^(studio_reuse|studio_design_contracts)$"
    if suite == "studio_designs":
        pattern = "^(studio_designs|studio_design_contracts|component_contracts|studio_quick|studio_colors|studio_color_interactions|studio_cutover)$"
    if suite == "studio_grid":
        pattern = "^(studio_grid|studio_grid_controls)$"
    if suite == "vpn":
        pattern = "^(vpn_setup|vpn_launch)$"
    if suite == "store":
        pattern = "^(isolde_tests|studio_selection|android_provisioning|web_remote|fdroid_store|store_lifecycle)$"
    if suite == "media_controls":
        pattern = "^(home_component_navigation|studio_navigation|kodi_media_client|kodi_local_bridge|kodi_cached_artwork|kodi_playback|kodi_import_contracts|native_profile_isolation)$"
    if suite == "campaign_repairs":
        pattern = "^(" + "|".join(sorted(REPAIR_TESTS)) + ")$"
    if suite == "runtime_contracts":
        pattern = "^(" + "|".join(sorted(RUNTIME_TESTS)) + ")$"
    if suite == "games":
        pattern = "^(component_contracts|games_remote_ui|isolde_tests|games_provisioning|steam_games)$"
    if suite == "sources":
        pattern = "^(studio_quick|isolde_tests|component_contracts|sources_remote_ui|home_component_navigation)$"
    if suite == "guide":
        pattern = "^(guide_contracts|guide_remote_ui|kodi_local_bridge|kodi_playback|kodi_media_client)$"
    if suite == "home_component_navigation":
        pattern = "^(home_component_navigation|studio_navigation)$"
    if suite == "kodi_media_client":
        pattern = "^(kodi_media_client|kodi_import_contracts|kodi_local_bridge)$"
    return targets, pattern
CASES = {"sources": ("home-components-acceptance.py", ["--studio", "--sources-only", "--inspect"]), "themecontrols": ("home-components-acceptance.py", ["--studio", "--themes-controls-only", "--inspect"]), "themecustom": ("home-components-acceptance.py", ["--studio", "--themes-custom-only", "--inspect"]), "themefaults": ("home-components-acceptance.py", ["--studio", "--themes-faults-only", "--inspect"]), "themes": ("home-components-acceptance.py", ["--studio", "--themes-only", "--inspect"]),
         "catalog": ("home-components-acceptance.py", ["--studio", "--catalog", "--inspect"]),
         "collections": ("home-components-acceptance.py", ["--studio", "--collections", "--inspect"]),
         "history": ("tv-library-acceptance.py", ["--history", "--inspect"]),
         "studio": ("home-components-acceptance.py", ["--studio", "--inspect"]),
         "studio-designs": ("home-components-acceptance.py", ["--studio", "--designs-only", "--inspect"]),
         "studio-search": ("home-components-acceptance.py", ["--studio", "--search-only", "--inspect"]),
         "desktop": ("home-components-acceptance.py", ["--studio", "--desktop", "--inspect"]),
         "lounge": ("home-reference-acceptance.py", ["--skin-from-settings", "--inspect"]),
         "pastel": ("home-reference-acceptance.py", ["--skin-from-settings", "--inspect"]),
         "repairs": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--inspect"]),
         "apps": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--verify-default-apps", "--inspect"]),
         "clients": ("home-reference-acceptance.py", ["--existing-profile", "8bdf6077-4613-4b52-9b20-d1ec7931dfcf", "--verify-default-apps", "--skip-steam", "--inspect"]),
         "local-search": ("tv-library-acceptance.py", ["--local-search", "--inspect"]),
         "pages": ("tv-library-acceptance.py", ["--pages", "--inspect"]),
         "guide": ("tv-library-acceptance.py", ["--guide", "--inspect"]),
         "guide-surfaces": ("tv-library-acceptance.py", ["--guide", "--guide-surfaces", "--inspect"]),
         "guide-lifecycle": ("tv-library-acceptance.py", ["--guide-lifecycle", "--inspect"]),
         "guide-features": ("tv-library-acceptance.py", ["--guide-features", "--inspect"]),
         "guide-soak": ("guide-soak-acceptance.py", []),
         "guide-reopen": ("guide-soak-acceptance.py", ["--cold-reopen"]),
         "movieart": ("tv-library-acceptance.py", ["--movie-art", "--inspect"]),
         "vpn": ("vpn-acceptance.py", []),
         "shell": ("shell-acceptance.py", []),
         "playlists": ("tv-library-acceptance.py", ["--playlists", "--inspect"]),
         "android": ("android-acceptance.py", []),
         "androidapps": ("android-apps-acceptance.py", []),
         "store": ("store-acceptance.py", ['--test-authorization']),
         "store-web": ("store-acceptance.py", ["--web-only"]),
         "store-offline": ("store-acceptance.py", ["--offline-matrix"]),
         "store-updates": ("store-acceptance.py", ["--android-updates","--test-authorization"]),
         "store-discovery": ("store-acceptance.py", ["--discovery-only"]),
         "games": ("games-acceptance.py", []),
         "emulators": ("games-acceptance.py", ["--emulator"])}


def secret_prompt_paths(tree):
    if len(tree)>65536:raise RuntimeError('Unbounded guest secret-service object inventory')
    paths=[line.strip() for line in tree.splitlines() if line.strip().startswith('/org/freedesktop/secrets/prompt/')]
    if len(paths)>16 or any(not re.fullmatch(r'/org/freedesktop/secrets/prompt/[A-Za-z0-9_]+',path) for path in paths):
        raise RuntimeError('Invalid guest secret-service prompt identity')
    return paths


def guest_notification_owner(pid, proc=Path('/proc')):
    if not isinstance(pid,int) or isinstance(pid,bool) or pid<=0:return False
    try:return (proc/str(pid)/'comm').read_text().strip() in {'gnome-shell','gsd-housekeepin'}
    except OSError:return False


def dismiss_guest_disk_banner():
    import gi
    gi.require_version('Atspi','2.0')
    from gi.repository import Atspi
    Atspi.set_timeout(100,100)
    pending=[(Atspi.get_desktop(0),[])];nodes=[];started=time.monotonic()
    while pending:
        if len(nodes)>=2048 or time.monotonic()-started>8:raise RuntimeError('Guest desktop accessibility inventory exceeded its bound')
        node,ancestors=pending.pop()
        if node is None:continue
        try:
            # Enumerate notifications, not every Isolde/Kodi/browser child.
            # Process identity is verified independently of accessible names.
            if len(ancestors)==1 and not guest_notification_owner(node.get_process_id()):continue
            # Hidden shell menus/history cannot obscure an installed window.
            # Avoid walking those potentially large, remote accessibility trees.
            if len(ancestors)>=2 and not node.get_state_set().contains(Atspi.StateType.SHOWING):continue
            name=(node.get_name() or '')[:256]
            index=len(nodes);nodes.append((node,name,ancestors))
            if len(ancestors)<12:
                count=node.get_child_count()
                if count>512:raise RuntimeError('Unbounded guest desktop accessibility children')
                pending.extend((node.get_child_at_index(i),ancestors+[index]) for i in range(count))
        except gi.repository.GLib.Error:continue
    warnings=[set(ancestors+[i]) for i,(_,name,ancestors) in enumerate(nodes) if 'low disk space' in name.lower()]
    for node,name,ancestors in nodes:
        if name!='Ignore':continue
        # Require a shared notification container, never the desktop/app root.
        if not any(any(index in warning for index in ancestors[-4:] if len(nodes[index][2])>=2) for warning in warnings):continue
        for button in [node]+[nodes[index][0] for index in reversed(ancestors[-2:])]:
            if button.get_role()!=Atspi.Role.PUSH_BUTTON:continue
            action=button.get_action_iface()
            if action and action.get_n_actions()>0 and action.do_action(0):
                return {'identifiedDiskWarning':True,'ignored':True}
            component=button.get_component_iface()
            if component and component.grab_focus() and button.get_state_set().contains(Atspi.StateType.FOCUSED):
                print('GUEST_DISK_IGNORE_FOCUSED',flush=True)
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    if button.get_state_set().contains(Atspi.StateType.DEFUNCT):
                        return {'identifiedDiskWarning':True,'ignored':True,'input':'focused-ignore-space'}
                    time.sleep(.1)
                raise RuntimeError('Identified guest Ignore button did not dismiss after bounded input')
        details=[]
        for target in [node]+[nodes[index][0] for index in reversed(ancestors[-4:])]:
            action=target.get_action_iface();component=target.get_component_iface()
            details.append({'role':target.get_role_name(),'actions':action.get_n_actions() if action else 0,'bounds':str(component.get_extents(Atspi.CoordType.SCREEN)) if component else None})
        raise RuntimeError('Could not dismiss the identified guest disk notification button: '+json.dumps(details))
    return {'identifiedDiskWarning':bool(warnings),'ignored':False}


def visual_result_passed(case, result):
    if case=='local-search':
        return (all(result.get(k) is True for k in ('passed','freshProfile','realKodi','mediaUnchanged'))
                and result.get('normalProfileUsed') is False and result.get('journey')=='local-search'
                and len(result.get('episodes',[]))==56 and len(result.get('shows',[]))>=1
                and result.get('network',{}).get('socketErrnos')==[101,101]
                and result.get('network',{}).get('unixSocketAvailable') is True
                and result.get('captures')==['localsearchorigin','localsearchentry','localsearchresults','localsearchmore','localsearchnext','localsearchprevious','localsearchplaying','localsearchplayreturn','localsearchreturned']
                and result.get('actualInputActions',0)>0)
    if case=='studio':
        colors=result.get('colorPicker',{})
        workflows=result.get('workflows',{})
        return (result.get('passed') is True and all(colors.get(k) is True for k in ('privateCancel','atomicApply','recentSaved','resetCancel','studioChrome','reopenPreserved'))
            and colors.get('captures')==['colorpreview','colorapplied','colorrecent','colorreset','chromepreview','chromeapplied','colorreopened']
            and all(workflows.get(k) is True for k in ('proportionalSize','sizeCancel','headerApply','headerRecovery','cameraNoWrite','resetCancel'))
            and workflows.get('captures')==['sizepreview','sizeapplied','headerpreview','headerorder','headerapplied','headerreopened','canvaszoom','resetcancel'])
    if case=='studio-search':
        network=result.get('network',{})
        if 'searchMatrixVersion' in result:
            phases=['searchhome','searchgallery','offlinemedia','offlinesearch','offlinewebon','offlineweboff','offlinewebpersisted','offlineprofilefresh','offlineprofileoff','offlineprofileon','offlinedefaulton','offlinewebsavefailed','offlinewebretry','offlinewebrestored','searchgalleryreturn','offlinerecovery']
            surfaces=result.get('ownedRuntimeWindows',[])
            if type(result['searchMatrixVersion'])!=int or result['searchMatrixVersion']!=2 or result.get('phases')!=phases:return False
            if not all(result.get(k) is True for k in ('webToggleVerified','coldPreferenceVerified','profileSwitchVerified','preferenceWriteFailureVerified','localKodiBoundaryVerified','galleryOriginVerified','layoutBytesUnchanged')):return False
            if not result.get('nativeKodiBefore') or result['nativeKodiBefore']!=result.get('nativeKodiAfter') or result.get('kodiBoundary')!='controlled-unix-rpc':return False
            if not isinstance(surfaces,list) or len(surfaces)!=4 or any(not isinstance(s,dict) or type(s.get('pid'))!=int or s['pid']<=0 or not re.fullmatch(r'0x[0-9a-f]+',str(s.get('window'))) for s in surfaces):return False
            if len({(s['pid'],s['window']) for s in surfaces})!=4:return False
        return (result.get('passed') is True and result.get('journey')=='studio-search' and result.get('freshProfile') is True and result.get('normalProfileUsed') is False and network.get('socketErrnos')==[101,101] and network.get('unixSocketAvailable') is True and result.get('actualInputActions',0)>0 and len(result.get('captureTrace',[]))>=3)
    if case=='studio-designs':
        designs=result.get('designs',{})
        required=('remoteSharedStyle','sharedImpactPreview','myComponents','linkedInsertDetach','freshIdentities','allFourTemplatesCustomized','currentPagePreservedRoot','namedMilestone','versionPreviewCancelNoWrite','defaultsApplyVersionRestoreExact','remotePortableExportImport','privateReferenceChooseHideRemove','coldReopenPreservedBytes','offlineSocketFilter')
        return (result.get('passed') is True and result.get('freshProfile') is True and result.get('normalProfileUsed') is False
                and result.get('journey')=='studio-designs' and all(designs.get(key) is True for key in required)
                and result.get('actualInputActions',0)>0 and isinstance(result.get('captureTrace'),list) and len(result['captureTrace'])>=20)
    if case=='catalog':
        return (all(result.get(k) is True for k in ('passed','freshProfile','remoteImport','invalidRefused','privateDraft','framingCancel','atomicGifApply','reopenPreserved','remotePrimitives','independentText','linkedImage','layersSelection'))
            and isinstance(result.get('managedGifSha256'),str) and re.fullmatch(r'[a-f0-9]{64}',result['managedGifSha256']) is not None
            and result.get('captures')==['catalogentry','imagestyles','imagebrowser','imageinvalid','imagepreview','imageplaced','imageinspector','imagemotiondraft','imageframing','imageframingdraft','imageapplied','catalogshape','catalogtext','textkeyboard','textindependent','catalogbutton','catalogcontainer','cataloglayers','imagelink','catalogapplied','imageruntimeone','imageruntimetwo','imagereopened'])
    if case=="history":
        return (all(result.get(key) is True for key in ("passed","freshProfile","realKodiPlayback","kodiOwnedLastplayed","historyCardPlayback","exactHistoryReturn","layoutUnchanged","mediaUnchanged"))
            and result.get("normalProfileUsed") is False
            and result.get("independentInstances")==[dict(count=5,capacity=3),dict(count=1,capacity=1)]
            and result.get("captures")==["historylibrary","historyplaying","historyupdated","historyfocused","historyreplaying","historyreturned","historyrepeatreturn"])
    if case == "collections":
        required=("passed","freshProfile","actualNativeInput","normalExit","exactGalleryReturn","buttonReturn","storeRemoveReinstall","layoutUnchanged")
        phases=["collectionhome","selectedgallery","collectionclient","selectedreturn","collectionrepeat","buttongallery","buttonclient","buttonreturn","buttonrepeat","storeclient","collectionuninstall","allremoved","selectedmissing","missingrefused","allrestored","reinstalledclient","reinstalledreturn"]
        if result.get("localFilterVerified") is True:
            phases.insert(2,"collectionfilter");phases.insert(phases.index("buttongallery"),"collectionfiltercleared")
        if 'galleryPresentationApplied' in result and (result['galleryPresentationApplied'] is not True or not isinstance(result.get('galleryPresentation'),dict) or not 1<=len(result['galleryPresentation'])<=10 or not re.fullmatch(r'[a-f0-9]{64}',str(result.get('galleryPresentationSha256')))):return False
        if result.get("appHistoryVerified") is True:
            if result.get("appHistorySessionOnly") is not True or result.get("appHistoryReopenCycles")!=2:return False
            phases += ["historyapp","historyclient","historyreturned","historyrepeat","historyrepeatreturn"]
        return (all(result.get(k) is True for k in required)
                and result.get("physicalHardwareVerified") is False
                and result.get("thirdPartyClientVerified") is False
                and result.get("captures")==phases)

    if case in {"guide-soak","guide-reopen"}:
        network=result.get('network',{})
        denied=(result.get('offlineSocketFilter') is True and network.get('mode')=='seccomp-af-inet-denied' and network.get('socketErrnos')==[101,101]
                and network.get('unixSocketAvailable') is True and network.get('rootOwners')=={'/':0,'/usr/lib/isolde-native/bwrap':0}
                and all(type(owner) is int for owner in network.get('rootOwners',{}).values()))
        inherited=network.get('mode')=='host-inherited' and result.get('offlineSocketFilter') in (None,False)
        mode=(result.get('realTwelveHourOff') is True and result.get('offlineNamespace') is True if case=='guide-soak' else result.get('actualColdReopen') is True and result.get('realTwelveHourOff') is False and result.get('offlineNamespace') is False and (denied or inherited))
        elapsed=(type(result.get('elapsedOffSeconds')) in (int,float) and 43200<=result['elapsedOffSeconds']<=7*86400) if case=='guide-soak' else result.get('elapsedOffSeconds') is None
        return (mode and all(result.get(key) is True for key in ('passed','actualLocalElapsedSeek','committedSevenDaySlotsUnchanged','automaticNativeReturn'))
                and result.get('normalProfileUsed') is False and elapsed
                and (case=='guide-soak' or result.get('explicitManagedStop') is True)
                and type(result.get('localSearchMatches')) is int and result['localSearchMatches']>0
                and result.get('captures')==['guidesoakhome','guidesoakguide','guidesoakvideo','guidesoakreturn','guidesoaksearch','guidesoakresults','guidesoakrecovery'])
    if case=="guide-features":
        phases=['guidefeedsetup','guidefeedpreview','guidefeedmapped','guidefeedimported','guidefeednolistings','guidefeedreturn','guidefeedauth','guidefeedoff','guidestationselect','guidestationlogo','guidestationcolor','guideappearanceapplied','guideexport','guideimportdraft','guideimportpreview','guideimported','guidetemplatesaved','guidetemplatereuse','guidecartoonrules','guidecalendarblock','guidecalendarsaved','guidemixedrules','guidemixedpreview','guidemixedcreated','guidedirectdraft','guidedirectpreview','guidedirectimported','guidefeedrefreshfailed']
        base=dict(result);base['captures']=result.get('captures',[])[:-len(phases)];base['cachedChannelCount']=3
        return (visual_result_passed('guide-surfaces',base) and result.get('captures',[])[-len(phases):]==phases and type(result.get('cachedChannelCount')) is int and result['cachedChannelCount']==10
                and all(result.get(key) is True for key in ('nativeFeedImport','sourceTimesRetained','explicitEpgMapping','actualNoEpgPlayback','authenticationFailureVisible','sourceOffPreservesCache','managedStationLogo','remoteColorWheel','portableArtwork','portableSchedule','savedRuleTemplate','remoteCalendarPolicy','cartoonNetworkOneSave','mixedNestedRules','directSourceImport','failedFeedRefreshPreservesCache')))
    if case=="guide-lifecycle":
        phases=['guidecontinuousready','guidecontinuousvideo','guidecontinuousnext','guidesurfed','guidesurfreturn','guidedeletedready','guidedeletedfile','guidedeletedreturn','guiderepairselect','guiderepaircontext','guiderepairreview','guiderepairconfirm','guiderepaired']
        base=dict(result);base['captures']=result.get('captures',[])[:-len(phases)]
        timings=result.get('cachedFirstPaintMs')
        return (visual_result_passed('guide-surfaces',base) and result.get('captures',[])[-len(phases):]==phases
                and all(result.get(key) is True for key in ('continuousTransition','remoteSurf','actualDeletedFile','targetedRepair','unaffectedSlotsPreserved'))
                and isinstance(timings,list) and 1<=len(timings)<=64 and all(type(value) is int and 0<=value<=1000 for value in timings))
    if case=="guide-surfaces":
        initial=["guidestartup","guideorigin","guidehome","guideempty","guidesource","guidepvr","guidevideo","guidereturn"]
        extra=["guidelocaldraft","guidelocalpreview","guidelocalreview","guidelocalcreated","guidelocalvideo","guidelocalosd","guidelocalreturn","guideduplicatedraft","guidelineup","guidewidgetentry","guidecompact","guideoverlay","guideexpanded","guideoverlayreturn","guidecompactreturn"]
        base=dict(result);base["captures"]=initial
        return (visual_result_passed("guide",base) and result.get("captures")==initial+extra
                and type(result.get("nativeChannelsCreated")) is int and result["nativeChannelsCreated"]==2
                and type(result.get("cachedChannelCount")) is int and result["cachedChannelCount"]==3
                and all(result.get(key) is True for key in ("surfaceJourneyInputAcknowledged","committedSlotsUnchanged","sourceCachePreserved","actualLocalElapsedSeek","automaticLocalReturn")))
    if case=="guide":
        return (all(result.get(k) is True for k in ("passed","freshProfile","remoteImport","actualPvrPlayback","returned","automaticNativeReturn","mediaUnchanged","syntheticSource"))
                and result.get("normalProfileUsed") is False and result.get("providerSubscriptionVerified") is False
                and result.get("pvrAddon")=="pvr.iptvsimple" and result.get("sourceChannels")==1 and result.get("sourceListings",0)>0
                and result.get("captures")==["guidestartup","guideorigin","guidehome","guideempty","guidesource","guidepvr","guidevideo","guidereturn"])
    if case == "emulators":
        return (all(result.get(key) is True for key in ("passed","freshProfile","remoteRuntimeSetup","remoteRegistration","nativeGameStarted","returned","sourceUnchanged"))
                and result.get("runtime")=="ppsspp" and result.get("gameplayVerified") is False
                and result.get("physicalControllerVerified") is False and result.get("captures")==[
                    "emulatorsempty","emulatorssetup","emulatorsinstallreview","emulatorsready","emulatorsadd","emulatorsreview",
                    "emulatorsdetails","emulatorspermission","emulatorsmenu","emulatorsplay","emulatorsaction","emulatorsreturn","emulatorsrestored"])
    if case == "movieart":
        return (all(result.get(key) is True for key in ("passed","freshProfile","scrapedByKodi","media_unchanged","freshSourceSetupThroughIsolde","sourcePreviewCancelNoWrite","nativeSourceWriteFailureRecovery","remotePosterPreviewNoWrite","remotePosterCancelNoWrite","remotePosterApplied"))
                and all(result.get(key) is False for key in ("normalProfileUsed","metadataSupplied"))
                and all(result.get(key) is True for key in ("playbackVerified","freshPlaybackReturnThroughIsolde","actualKodiResumeSavedAndRestored","preferenceOffLibraryDirect","remoteHomeMoviesRoute","realAudioTrackSelection","sourceEditPreservesProgress","remoteMusicSourceBrowsePlayback"))
                and result.get("kodiMenusRequired") is False
                and result.get("artworkBytes",0)>4096)
    if case == "games":
        return (all(result.get(key) is True for key in ("passed","freshProfile","remoteRegistration","nativeGameStarted","returned"))
                and result.get("gameplayVerified") is False and result.get("physicalControllerVerified") is False and len(result.get("captures",[]))==9)
    if case == "store":
        return (all(result.get(k) is True for k in ("passed","androidLifecycle","chromiumLifecycle","androidPackageRemovalVerified","freshProfile"))
                and result.get("normalProfileUsed") is False and result.get("playbackVerified") is False
                and result.get('testAuthorization') is True and result.get('authorizationCleanupVerified') is True
                and result.get('androidConsentCancelNoWrite') is True and result.get('manifestsRemoved') is True
                and result.get('actualInputActions',0)>0
                and result.get('captures')==['storeandroid','storeandroidconsent','storeandroidinstalled','storeandroidmenu','storeandroidcontent','storeandroidopen','storeandroidclosed','storeandroidremove','storeandroidremoved','storechromium','storechromiumconsent','storechromiuminstalled','storechromiumopen','storechromiumclosed','storechromiumremove','storechromiumremoved'])
    if case == 'store-updates':
        return (all(result.get(key) is True for key in ('passed','freshProfile','signedBaselineFixture','actualStoreUpdate','preparedCrashRollback','savedDataPreserved','subscriptionPersisted','profileIsolation','packageRemovalVerified','authorizationCleanupVerified'))
            and result.get('normalProfileUsed') is False and result.get('actualInputActions',0)>0
            and result.get('captures')==['updatebaseline','updateoffer','updateconsent','updateprogress','updateinstalled','updatesunsubscribed','updateprofilefresh','updatecold','updateremove','updateremoved'])
    if case == 'store-offline':
        proofs=result.get('networkProofs',[])
        return (all(result.get(k) is True for k in ('passed','freshProfile','profileIsolation','coldReopen','cancelNoWrite','writeFailureRollback','retryVerified','webLifecycle','repositoryFailureRecovered','manifestsRemoved'))
            and result.get('normalProfileUsed') is False and result.get('androidLifecycle') is False and result.get('actualInputActions',0)>0
            and len(proofs)==3 and all(p.get('socketErrnos')==[101,101] and p.get('unixSocketAvailable') is True for p in proofs)
            and len({p.get('pid') for p in proofs})==3
            and result.get('captures')==['offlineentry','offlinecancel','offlinewritefailed','offlineainstalled','offlinebfresh','offlinebinstalled','offlinebremoved','offlineacold','offlinewebopen','offlinewebreturn','offlinearemove','offlineremoved','offlinerepositoryerror','offlineaccessoff','offlinesettings'])
    if case == 'store-web':
        return (all(result.get(k) is True for k in ('passed','chromiumLifecycle','webOnly','freshProfile','manifestsRemoved'))
            and result.get('normalProfileUsed') is False and result.get('androidLifecycle') is False
            and result.get('actualInputActions',0)>0 and result.get('captures')==['storechromium','storechromiumconsent','storechromiuminstalled','storechromiumopen','storechromiumclosed','storechromiumremove','storechromiumremoved'])
    if case == "store-discovery":
        return (all(result.get(k) is True for k in ("passed","freshProfile","catalogOnly")) and result.get("journey")=="store-discovery" and result.get("normalProfileUsed") is False and result.get("installedAppsChanged") is False
                and result.get("provider")=="fdroid-signed-v2" and result.get("query")=="newpipe" and bool(result.get("matches")) and result.get("actualInputActions",0)>0
                and all(re.fullmatch(r'[a-f0-9]{64}',str(result.get(k,''))) for k in ('indexSha256','entrySha256'))
                and result.get("captures")==['storediscoveryentry','storediscoveryloading','storediscoverybrowse','storediscoverykeyboard','storediscoveryresults','storediscoveryreturn','storediscoverysettingsfocus','storediscoverysettings'])
    if case == "androidapps":
        return (all(result.get(k) is True for k in ("passed", "sharedSessionLaunch", "ownedSessionStopped"))
                and result.get("playbackVerified") is False
                and result.get("reviewSequenceCaptured")==["androidnavigation","androidgallery","androidlaunch","androidpermission","androidopen","androidremote","androidreturn"])
    if case == "android":
        return (all(result.get(k) is True for k in ("passed", "compatibilityProbeOnly", "userdataBindingVerified", "packageInstalled", "launchRequested", "ownedSessionStopped"))
                and all(result.get(k) is False for k in ("isoldeIntegrationVerified", "playbackVerified", "remoteVerified")))
    if case == "playlists":
        return (all(result.get(key) is True for key in ("passed","real_kodi_saved_playlist","playlist_creation_ui_verified","playlist_cancel_no_write","play_pause_stop","media_unchanged","rename_collision_preserved","remote_rule_preview_confirm","smart_sort_saved","member_reorder_remote_cancel_commit","delete_cancel_confirm","nativeMenuOwnedIsoldeControls","remotePictureSize","remoteSubtitleOff","realNextPrevious","controlsCloseRestoresVideoFocus"))
                and result.get("normal_profile_used") is False and result.get("picker_ui_verified") is False and result.get("episodes")==3)
    if case == "shell":
        return (result.get("passed") is True and result.get("reviewSequenceCaptured")==["shellnavigation","shellgallery","shellstore","shellreturn","shellambient","shellwake"]
                and result.get("hostUiAutomationUsed") is False and result.get("layoutWrites") is False and result.get("semanticAcceptance")=="not_verified")
    if case == "vpn":
        return (all(result.get(k) is True for k in ("passed", "defaultProvidersInstalled", "uiAuthoredRule", "countryVerified", "nativeClientOpened"))
                and result.get("app")=="cbc-gem" and result.get("country")=="CA"
                and result.get("browserUsedForSetup") is False and result.get("hostUiAutomationUsed") is False)
    if case == "clients":
        return all(result.get(key) is True for key in ("passed", "native_clients", "household_unchanged")) and result.get("steam_client_downloaded") is False and result.get("subscriptionPlaybackVerified") is False
    if case == "apps":
        return all(result.get(key) is True for key in ("passed", "native_clients", "steam_client_downloaded", "household_unchanged")) and result.get("subscriptionPlaybackVerified") is False
    if case == "repairs":
        return all(result.get(key) is True for key in ("passed", "preserving_update", "household_unchanged", "real_compositor_return"))
    if case in ("lounge", "pastel"):
        functional = (result.get("functional_journey_passed") is True
                      if "functional_journey_passed" in result else
                      result.get("component_support") is True and result.get("missing") == [])
        return (functional
                and result.get("reset_verified") is True
                and result.get("settings_skin_round_trip") is True
                and result.get("saved_skin_survives_reset") is True)
    if case == "sources":
        phases={"sourcesdefault","sourceslist","sourcesdraft","sourceshidden","sourcesrestored","sourceshandlers","sourcesdisconnected","sourcesretry","sourcespolicy","sourcespolicysaved","sourceskeep","sourceskept","darksourcesbounded","darksourcesactions","darksourcesnotice","pastelsourcesbounded","pastelsourcesactions","pastelsourcesnotice"}
        return (result.get("journey")==case and set(result.get("phases",[]))==phases
                and all(result.get(key) is True for key in ("passed","freshProfile","noUnintendedLayoutWrites","explicitPolicySave","remoteHandlerSaved","remoteKeepSaved","fixtureOnly","remoteHideRestore","policyCancelNoWrite"))
                and result.get("normalProfileUsed") is False)
    if case == "themecontrols":
        phases={tag+phase for tag in ("dark","pastel","custom") for phase in ("audio","ambient","return")} | {"custompolicy","customreset","customcancel"}
        if "soundAcceptanceVersion" in result:
            if type(result["soundAcceptanceVersion"])!=int or result["soundAcceptanceVersion"]!=1:return False
            phases |= {tag+phase for tag in ("dark","pastel","custom") for phase in ("sounds","soundpreview","sounddisabled")}
            observations=result.get("soundOutput",[])
            expected={(tag,kind) for tag in ("dark","pastel","custom") for kind in ("preview","disabled")}
            if not isinstance(observations,list) or len(observations)!=6 or {(o.get("palette"),o.get("case")) for o in observations if isinstance(o,dict)}!=expected:return False
            for observation in observations:
                if not isinstance(observation,dict):return False
                size=observation.get("bytes");peak=observation.get("peak");nonzero=observation.get("nonzero")
                if type(size)!=int or not 96000<=size<=2*1024*1024 or size%2 or type(peak)!=int or type(nonzero)!=int:return False
                if not re.fullmatch(r"[a-f0-9]{64}",str(observation.get("sha256",""))):return False
                if observation["case"]=="preview" and not (50<peak<=32768 and 100<nonzero<=size//2):return False
                if observation["case"]=="disabled" and (peak!=0 or nonzero!=0):return False
        return (result.get("passed") is True and result.get("journey")==case
            and result.get("freshProfile") is True and result.get("normalProfileUsed") is False
            and set(result.get("phases",[]))==phases and result.get("palettes")==["dark","pastel","custom"]
            and result.get("layoutBytesUnchanged") is True
            and bool(result.get("nativeKodiBefore")) and result.get("nativeKodiBefore")==result.get("nativeKodiAfter"))
    if case == "themefaults":
        phases={tag+phase for tag in ("dark","pastel","custom") for phase in ("gallery","loading","error","retry","return")}
        return (result.get("passed") is True and result.get("journey")=="themefaults"
            and result.get("freshProfile") is True and result.get("normalProfileUsed") is False
            and set(result.get("phases",[]))==phases and result.get("palettes")==["dark","pastel","custom"]
            and result.get("controlledFaults")==3 and result.get("layoutBytesUnchanged") is True
            and set(result.get("loadingFrames",{}))=={tag+"loading-frame.png" for tag in ("dark","pastel","custom")}
            and all(re.fullmatch(r"[0-9a-f]{64}",v) for v in result["loadingFrames"].values())
            and bool(result.get("nativeKodiBefore")) and result.get("nativeKodiBefore")==result.get("nativeKodiAfter"))
    if case in {"themes","themecustom"}:
        phases=("home","origin","search","searchkeyboard","searchreturn","gallery","galleryreturn","movies","details","shows","seasons","episodes","network","networkkeyboard","sources","remotes","profiles","vpn","storepreferences","system","display","audio","ambient","media","folders","import","components","inputs","apps","appdialog","appkeyboard","return")
        palettes=["custom"] if case=="themecustom" else ["dark","pastel","custom"]
        required={tag+phase for tag in palettes for phase in phases} | {"custompolicy","customreset","customcancel"}
        return (result.get("passed") is True and result.get("journey")==case
                and result.get("freshProfile") is True and result.get("normalProfileUsed") is False
                and result.get("palettes")==palettes
                and required.issubset(set(result.get("phases",[])))
                and result.get("layoutBytesUnchanged") is True
                and bool(result.get("nativeKodiBefore")) and result.get("nativeKodiBefore")==result.get("nativeKodiAfter"))
    return result.get("passed") is True


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def android_admin_launcher(command, run, title):
    return ("import subprocess,json\nprint("+repr(title+" — enter the VM administrator password only here.")+",flush=True)\n"
        +"p=subprocess.Popen("+repr(["pkexec"]+command)+",stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)\n"
        +"total=0\nwith open("+repr(str(run/"android-admin.log"))+",'w') as log:\n"
        +" for line in p.stdout:\n  print(line[:8192],end='',flush=True)\n  if total<8*1024*1024: log.write(line[:8192]);log.flush();total+=len(line[:8192])\n"
        +"code=p.wait()\nwith open("+repr(str(run/"android-admin-exit.json"))+",'w') as result:json.dump({'exit':code},result)\nraise SystemExit(code)\n")


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def verified_reopen_helper(root, expected):
    return verified_guide_helper(root,expected,'reopen')


def verified_guide_helper(root, expected, kind):
    if kind not in {'reopen','features'}:
        raise RuntimeError('Unsupported Guide helper kind')
    if not isinstance(expected,str) or not re.fullmatch(r'[0-9a-f]{64}',expected):
        raise RuntimeError('Guide requires an exact helper digest')
    path=root/'bin'/('guide-'+kind+'-'+expected+'.py')
    if path.is_symlink() or path.resolve()!=path or not path.is_file() or digest(path)!=expected:
        raise RuntimeError('Cold-reopen helper identity mismatch')
    return path


def feature_helper_command(source, helper, executable, flags):
    bootstrap="import sys,runpy,importlib.util;sys.path.insert(0,sys.argv.pop(1));helper=sys.argv.pop(1);spec=importlib.util.spec_from_file_location('guide_features',helper);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);sys.modules['guide_features']=module;path=sys.argv[1];sys.argv=sys.argv[1:];runpy.run_path(path,run_name='__main__')"
    return ['python3','-c',bootstrap,str(source/'tools'),str(helper),str(source/'tools'/'tv-library-acceptance.py'),str(executable)]+flags


def verified_studio_helper(root, expected, kind='studio'):
    if kind not in {'studio','history','reference','reference-collections','store'}:raise RuntimeError('Unsupported bounded helper kind')
    if not isinstance(expected,str) or not re.fullmatch(r'[0-9a-f]{64}',expected):raise RuntimeError('Studio requires an exact helper digest')
    path=root/'bin'/(kind+'-helper-'+expected+'.py')
    if path.is_symlink() or path.resolve()!=path or not path.is_file() or digest(path)!=expected:raise RuntimeError('Studio helper identity mismatch')
    return path


def studio_helper_command(source, helper, executable, desktop=False, collections=False, catalog=False, designs=False, search=False, themes=False, themefaults=False, themecustom=False, themecontrols=False, sources=False, gallery_evidence=None, gallery_digest=None):
    if gallery_evidence is not None and (not collections or not re.fullmatch(r'[a-f0-9]{64}',str(gallery_digest))):raise RuntimeError('Gallery evidence is collections-only and requires an exact digest')
    bootstrap="import sys;from pathlib import Path;sys.path.insert(0,sys.argv.pop(1));helper=sys.argv.pop(1);frozen=sys.argv.pop(1);sys.argv=[frozen]+sys.argv[1:];exec(compile(Path(helper).read_bytes(),frozen,'exec'),{'__name__':'__main__','__file__':frozen})"
    return ['python3','-c',bootstrap,str(source/'tools'),str(helper),str(source/'tools/home-components-acceptance.py'),str(executable),'--studio']+(['--desktop'] if desktop else [])+(['--collections'] if collections else [])+(['--catalog'] if catalog else [])+(['--designs-only'] if designs else [])+(['--search-only'] if search else [])+(['--themes-only'] if themes else [])+(['--themes-faults-only'] if themefaults else [])+(['--themes-custom-only'] if themecustom else [])+(['--themes-controls-only'] if themecontrols else [])+(['--sources-only'] if sources else [])+(['--gallery-presentation-evidence',str(gallery_evidence),'--gallery-presentation-digest',gallery_digest] if gallery_evidence is not None else [])+['--inspect']


def verified_gallery_evidence(run, result):
    record=result.get('galleryPresentationEvidence')
    if record is None:return None,None
    if result.get('passed') is not True or not isinstance(record,dict) or record.get('file')!='studio-gallery-presentation.json' or not re.fullmatch(r'[a-f0-9]{64}',str(record.get('sha256'))):raise RuntimeError('Invalid native gallery evidence receipt')
    path=run/record['file']
    if path.resolve()!=path or not path.is_file() or path.stat().st_size>65536 or digest(path)!=record['sha256']:raise RuntimeError('Native gallery evidence identity mismatch')
    value=json.loads(path.read_text())
    if not isinstance(value,dict) or not 1<=len(value)<=10:raise RuntimeError('Invalid native gallery presentation')
    return path,record['sha256']


def history_helper_command(source, helper, executable, pages=False, playlists=False, game_case=None, local_search=False, movie_art=False):
    if int(pages)+int(playlists)+int(game_case is not None)+int(local_search)+int(movie_art)>1 or game_case not in (None,'games','emulators'):raise ValueError("Select one library journey")
    bootstrap="import sys;from pathlib import Path;sys.path.insert(0,sys.argv.pop(1));helper=sys.argv.pop(1);frozen=sys.argv.pop(1);sys.argv=[frozen]+sys.argv[1:];exec(compile(Path(helper).read_bytes(),frozen,'exec'),{'__name__':'__main__','__file__':frozen})"
    frozen='games-acceptance.py' if game_case else 'tv-library-acceptance.py'
    flags=(['--emulator'] if game_case=='emulators' else []) if game_case else ['--local-search' if local_search else '--pages' if pages else '--playlists' if playlists else '--movie-art' if movie_art else '--history','--inspect']
    return ['python3','-c',bootstrap,str(source/'tools'),str(helper),str(source/'tools'/frozen),str(executable)]+flags


def store_helper_command(source, helper, executable, flags):
    bootstrap="import sys;from pathlib import Path;sys.path.insert(0,sys.argv.pop(1));helper=sys.argv.pop(1);frozen=sys.argv.pop(1);sys.argv=[frozen]+sys.argv[1:];exec(compile(Path(helper).read_bytes(),frozen,'exec'),{'__name__':'__main__','__file__':frozen})"
    return ['python3','-c',bootstrap,str(source/'tools'),str(helper),str(source/'tools/store-acceptance.py'),str(executable)]+flags


def reference_helper_command(source, helper, executable, flags, collections_helper=None):
    bootstrap="import sys;from pathlib import Path;sys.path.insert(0,sys.argv.pop(1));helper=sys.argv.pop(1);frozen=sys.argv.pop(1);collections=sys.argv.pop(1);"
    bootstrap+="import importlib.util;"
    bootstrap+="spec=importlib.util.spec_from_file_location('reference_collections',collections) if collections else None;"
    bootstrap+="module=importlib.util.module_from_spec(spec) if spec else None;"
    bootstrap+="exec(compile(Path(collections).read_bytes(),str(Path(frozen).with_name('reference_collections.py')),'exec'),module.__dict__) if module else None;"
    bootstrap+="sys.modules.update({'reference_collections':module}) if module else None;"
    bootstrap+="sys.argv=[frozen]+sys.argv[1:];exec(compile(Path(helper).read_bytes(),frozen,'exec'),{'__name__':'__main__','__file__':frozen})"
    return ['python3','-c',bootstrap,str(source/'tools'),str(helper),str(source/'tools/home-reference-acceptance.py'),str(collections_helper) if collections_helper else '',str(executable)]+flags


def guide_backend_diagnostics(run):
    wrapper=run/'guide-soak-receipt.json'
    if not wrapper.is_file():return None
    receipt_path=Path(json.loads(wrapper.read_text())['receipt'])
    if receipt_path.is_symlink() or not re.fullmatch(r'/tmp/isolde-tv-acceptance-[A-Za-z0-9_-]+/guide-soak-ready.json',str(receipt_path)):
        raise RuntimeError('Unsafe Guide diagnostic receipt')
    receipt=json.loads(receipt_path.read_text());profile=receipt['profile']
    if not re.fullmatch(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}',profile):raise RuntimeError('Invalid Guide diagnostic profile')
    database=receipt_path.parent/'config/Isolde/profiles'/profile/'guide.sqlite'
    if str(database)!=receipt.get('database') or database.resolve()!=database:raise RuntimeError('Unsafe Guide diagnostic database')
    import sqlite3
    from contextlib import closing
    with closing(sqlite3.connect('file:'+str(database)+'?mode=ro',uri=True)) as connection:
        current_slots=list(connection.execute('SELECT id,channel,media_type,media_id,title,start_utc,end_utc,status FROM slots ORDER BY channel,start_utc'))
        by_id={row[0]:list(row) for row in current_slots};expected=receipt['nativeSlots']
        clock=time.time();now=[row for row in current_slots if row[5]<=clock<row[6]]
        slot_proof={'compared':len(expected),'committedUnchanged':all(by_id.get(row[0])==row for row in expected),'currentPrograms':now[:16]}
    base=Path.home()/'.local/share/Isolde/native-profiles'/profile/'kodi'
    logs={}
    for name in ('home/.kodi/temp/kodi.log','home/.cache/kodi/kodi.log','home/.cache/kodi.log'):
        path=base/name
        if path.is_file() and path.resolve().is_relative_to(base.resolve()):
            with path.open('rb') as stream:stream.seek(max(0,path.stat().st_size-8192));logs[name]=stream.read(8192).decode('utf-8','replace')
    sandbox=Path('/usr/lib/isolde-native/bwrap');probe={}
    if sandbox.is_file() and all(not p.is_symlink() and p.stat().st_uid==0 and not (p.stat().st_mode & 0o022) for p in (sandbox,*sandbox.parents)):
        code='import os,json;print(json.dumps({"uid":os.getuid(),"uidMap":open("/proc/self/uid_map").read(),"sandboxOwner":os.stat("/usr/lib/isolde-native/bwrap").st_uid,"rootOwner":os.stat("/").st_uid}))'
        for name,flags in (('userAndNetwork',['--unshare-user','--unshare-net']),('networkOnly',['--unshare-net'])):
            result=subprocess.run([str(sandbox),*flags,'--cap-drop','ALL','--bind','/','/','--proc','/proc','--','python3','-c',code],capture_output=True,text=True,timeout=10)
            probe[name]={'exit':result.returncode,'output':result.stdout[-1024:],'error':result.stderr[-1024:]}
    privilege=subprocess.run(['sudo','-n','-l'],capture_output=True,text=True,timeout=5)
    return {'profile':profile,'rootExists':base.is_dir(),'backendLogs':logs,'slots':slot_proof,'namespaceIdentityProbe':probe,'guestTestPrivileges':privilege.stdout[-4096:],'sandboxMode':oct(sandbox.stat().st_mode & 0o7777) if sandbox.exists() else None}


def safe_relative(name):
    p = PurePosixPath(name)
    if not name or "\\" in name or p.is_absolute() or any(v in (".", "..") for v in name.split("/")):
        raise RuntimeError("Unsafe snapshot path: " + name)
    return p


def manifest(path):
    entries = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        if not re.fullmatch(r"[0-9a-f]{64}  .+", line):
            raise RuntimeError("Invalid source hash manifest")
        name = line[66:]
        safe_relative(name)
        if name in entries:
            raise RuntimeError("Duplicate snapshot path: " + name)
        entries[name] = line[:64]
    if not entries:
        raise RuntimeError("Empty source manifest")
    return entries


def verify_tree(root, entries):
    for name, expected in entries.items():
        path = root / name
        if path.is_symlink() or not path.is_file() or digest(path) != expected:
            raise RuntimeError("Source changed or missing: " + name)


def extract_snapshot(run):
    expected = (run / "archive.sha256").read_text().split()[0]
    if digest(run / "source.tar.gz") != expected:
        raise RuntimeError("Archive checksum mismatch; do not build")
    entries = manifest(run / "files.sha256")
    source = run / "source"
    if source.exists():
        raise RuntimeError("Snapshot already extracted; use Status, Visual or Deploy for this run")
    source.mkdir()
    with tarfile.open(run / "source.tar.gz", "r:gz") as archive:
        seen = set()
        for member in archive:
            name = str(safe_relative(member.name))
            if name not in entries or name in seen or not member.isfile():
                raise RuntimeError("Unlisted, duplicate or non-regular archive entry: " + name)
            seen.add(name)
            target = source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with archive.extractfile(member) as incoming, target.open("wb") as output:
                shutil.copyfileobj(incoming, output)
            target.chmod(member.mode & 0o777)
        if seen != set(entries):
            raise RuntimeError("Archive and manifest disagree")
    verify_tree(source, entries)
    return source, entries


def remove_guest_source_snapshot(candidate, runs, mounts):
    """Remove a reproducible extracted copy, preserving its verified archive."""
    candidate, runs = Path(candidate), Path(runs)
    source = candidate / "source"
    if (runs.resolve() != runs or candidate.parent != runs or candidate.resolve() != candidate
            or source.resolve() != source or not source.is_dir()):
        raise RuntimeError("Unsafe guest source snapshot path")
    if any(m == source or m.is_relative_to(source) or (m != Path("/") and source.is_relative_to(m)) for m in mounts):
        raise RuntimeError("Refusing mounted guest source snapshot")
    for name in ("source.tar.gz", "archive.sha256", "files.sha256"):
        path = candidate / name
        if path.is_symlink() or not path.is_file():
            raise RuntimeError("Missing regular recovery archive or manifest")
    archive_path = candidate / "source.tar.gz"
    if archive_path.stat().st_size > 256 * 1024**2:
        raise RuntimeError("Recovery archive exceeds maintenance bounds")
    expected = (candidate / "archive.sha256").read_text().split()
    if not expected or not re.fullmatch(r"[0-9a-f]{64}", expected[0]) or digest(archive_path) != expected[0]:
        raise RuntimeError("Recovery archive checksum mismatch")
    entries = manifest(candidate / "files.sha256")
    if len(entries) > 10000:
        raise RuntimeError("Recovery manifest exceeds maintenance bounds")
    seen, total = set(), 0
    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive:
            name = str(safe_relative(member.name))
            total += member.size
            if name not in entries or name in seen or not member.isfile() or total > 1024**3:
                raise RuntimeError("Invalid bounded recovery archive entry")
            with archive.extractfile(member) as incoming:
                actual = hashlib.file_digest(incoming, "sha256").hexdigest()
            if actual != entries[name]:
                raise RuntimeError("Recovery archive member checksum mismatch")
            seen.add(name)
    if seen != set(entries):
        raise RuntimeError("Recovery archive and manifest disagree")
    verify_tree(source, entries)
    for path in source.rglob("*"):
        if path.is_symlink():
            raise RuntimeError("Refusing linked extracted source")
        if path.is_file() and path.relative_to(source).as_posix() not in entries:
            if path.suffix != ".pyc" or path.parent.name != "__pycache__":
                raise RuntimeError("Unrecorded extracted source file; preserve for inspection")
    shutil.rmtree(source)

def restore_source_archive(candidate, bins):
    record=candidate/'archive.sha256';target=candidate/'source.tar.gz'
    if target.exists() or target.is_symlink():return False
    if record.is_symlink() or not record.is_file():return False
    expected=record.read_text().split()[0]
    if not re.fullmatch(r'[a-f0-9]{64}',expected):raise RuntimeError('Unsafe saved archive identity')
    archive=bins/('recovery-'+expected+'.tar.gz')
    if not archive.exists():return False
    if archive.resolve()!=archive or archive.stat().st_size>256*1024**2 or digest(archive)!=expected:raise RuntimeError('Recovery archive identity mismatch')
    archive.rename(target)
    return True


def sync_workspace(source, destination, entries, previous):
    """Keep timestamps on identical files so Ninja can reuse compiled objects."""
    destination.mkdir(parents=True, exist_ok=True)
    base = destination.resolve()
    changed = 0
    for name in set(previous) - set(entries):
        target = destination / safe_relative(name)
        if not target.resolve().is_relative_to(base) or target.is_symlink():
            raise RuntimeError("Unsafe stale workspace file: " + name)
        if target.is_file():
            target.unlink()  # Only paths from our previous verified manifest; never recursive.
    for name, expected in entries.items():
        target = destination / name
        if not target.resolve().is_relative_to(base) or target.is_symlink():
            raise RuntimeError("Unsafe workspace destination: " + name)
        if target.is_file() and digest(target) == expected:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / name, target)
        os.utime(target, None)  # Changed files must be newer than cached objects.
        changed += 1
    verify_tree(destination, entries)
    return changed


def payload_hashes(payload):
    result = {}
    for path in sorted(payload.rglob("*")):
        if path.is_symlink():
            raise RuntimeError("Payload contains a symlink")
        if path.is_file():
            result[path.relative_to(payload).as_posix()] = digest(path)
    if "bin/isolde" not in result:
        raise RuntimeError("Missing staged executable")
    return result


def remove_generated_payload_cache(run, payload=None):
    """Restore exact tested bytes; never ignore extras in integrity checks."""
    record=run/'test-result.json';payload=payload if payload is not None else run/'payload'
    if not record.is_file() or not payload.is_dir():return []
    if run.resolve()!=run or payload.resolve()!=payload:raise RuntimeError('Redirected payload cache maintenance')
    tested=json.loads(record.read_text());expected=tested.get('payload',{})
    if tested.get('passed') is not True or not (tested.get('fullSuite') is True or tested.get('coverageReuse')):return []
    actual=payload_hashes(payload)
    if any(actual.get(name)!=value for name,value in expected.items()):raise RuntimeError('Tested payload bytes changed; cache cleanup refused')
    candidates=[]
    for name in sorted(set(actual)-set(expected)):
        path=payload/name;match=re.fullmatch(r'([A-Za-z0-9_-]+)\.cpython-[0-9]+(?:\.opt-[12])?\.pyc',path.name)
        if path.parent.name!='__pycache__' or not match or path.stat().st_size>4*1024*1024:
            raise RuntimeError('Unrecognized extra payload file; cache cleanup refused')
        source=path.parent.parent/(match[1]+'.py');source_name=source.relative_to(payload).as_posix()
        if source_name not in expected or actual.get(source_name)!=expected[source_name]:raise RuntimeError('Cache source is not a verified tested module')
        candidates.append({'path':str(path),'sha256':actual[name],'source':source_name,'sourceSha256':expected[source_name]})
    if candidates:
        atomic_json(run/('guest-python-cache-cleanup.json' if payload==run/'payload' else 'guest-installed-python-cache-cleanup.json'),{'targets':candidates,'testedFilesModified':False})
        for candidate in candidates:Path(candidate['path']).unlink()
        if payload_hashes(payload)!=expected:raise RuntimeError('Payload did not return to its exact tested identity')
    return candidates


def stop_owned(process):
    if process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=8)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def write_soak_receipt(run, evidence, executable):
    receipts=[p/'guide-soak-ready.json' for p in evidence if (p/'guide-soak-ready.json').is_file()]
    if len(receipts)!=1:raise RuntimeError('Missing unique twelve-hour-off preparation receipt')
    atomic_json(run/'guide-soak-receipt.json',{'receipt':str(receipts[0]),'sourceDigest':digest(run/'files.sha256'),'executableSha256':digest(executable)})


def original_full_payload_matches(base, result):
    """A pruned staged copy may use its exact retained installed release bytes."""
    payload = base / "payload"
    if payload.is_symlink() or payload.resolve() != payload:
        raise RuntimeError("Original full staged payload path is redirected")
    if payload.exists():
        return payload_hashes(payload) == result.get("payload")
    record = base / "deploy-result.json"
    if record.is_symlink() or not record.is_file():
        raise RuntimeError("Original full payload and installed receipt are missing")
    release = Path(json.loads(record.read_text())["release"])
    releases = Path.home() / ".local/share/isolde-appliance/releases"
    if (release.parent != releases or not re.fullmatch(r"[0-9a-f]{24}", release.name)
            or release.resolve() != release or not release.is_dir()):
        raise RuntimeError("Original full installed payload path is unsafe or missing")
    return payload_hashes(release) == result.get("payload")


def qml_coverage(root, run, base_id, suite, expected_suite, qml, harness, label, added_harness=frozenset()):
    """Verify original full coverage and an explicit bounded QML delta."""
    if suite!=expected_suite or not isinstance(base_id,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',base_id):
        raise RuntimeError(label+' QML reuse requires '+expected_suite+' and an exact base run')
    base=root/'runs'/base_id
    if base==run or base.is_symlink() or base.resolve()!=base:
        raise RuntimeError('Unsafe full-suite base run')
    result=json.loads((base/'test-result.json').read_text())
    if result.get('passed') is not True or result.get('fullSuite') is not True or result.get('sourceDigest')!=digest(base/'files.sha256'):
        raise RuntimeError('Guide QML reuse requires a passing original full suite')
    if not original_full_payload_matches(base,result):
        raise RuntimeError('Full-suite base payload changed')
    before=manifest(base/'files.sha256');after=manifest(run/'files.sha256')
    changed=sorted(name for name in before.keys()|after.keys() if before.get(name)!=after.get(name))
    outside=[name for name in changed if name not in qml|harness and not name.startswith('docs/')]
    if not qml.intersection(changed) or outside:
        raise RuntimeError('Source changes exceed the focused '+label+' QML coverage boundary: '+(', '.join(outside[:16]) if outside else 'no covered application change'))
    if any(name not in after or (name not in before and name not in added_harness) for name in changed):
        raise RuntimeError('Focused reuse cannot add or remove source files outside its named harness boundary')
    return {'baseRunId':base_id,'baseSourceDigest':result['sourceDigest'],
            'sourceDigest':digest(run/'files.sha256'),'changedPaths':changed,'changed'+label+'Qml':sorted(qml.intersection(changed))}


def guide_qml_coverage(root, run, base_id, suite):
    return qml_coverage(root,run,base_id,suite,'guide_remote_ui',{'qml/GuideWidget.qml','qml/GuideView.qml'},
        {'tests/guide_ui.cpp','tests/test_vm_oracle.py','tests/test_kodi_bridge.py',
         'tools/vm-test.ps1','tools/vm-oracle.py','tools/guide-soak-acceptance.py','tools/guide-reopen-inputs.json','tools/tv-library-acceptance.py',
         'tools/guide-input-plan.ps1','tools/test-guide-input-plan.ps1'},'Guide')


def search_origin_main_delta(before,after):
    edits=[('        property string themeOrigin: ""\n        movieSearchClient:',
            '        property string themeOrigin: ""\n        property bool playbackOriginLibraryOpen: false\n        movieSearchClient:'),
           ('        onMediaRequested: (mediaType, mediaId, query, collectionId, pageStart) => {\n            kodiLibrary.open = true',
            '        onMediaRequested: (mediaType, mediaId, query, collectionId, pageStart) => {\n            commandCenter.playbackOriginLibraryOpen = kodiLibrary.open\n            kodiLibrary.open = true'),
           ('function onSearchReturnRequested() { commandCenter.open = true }',
            'function onSearchReturnRequested() { kodiLibrary.open = commandCenter.playbackOriginLibraryOpen; commandCenter.open = true }')]
    for old,new in edits:
        if before.count(old)!=1:raise RuntimeError('Unrecognized original Search origin routing')
        before=before.replace(old,new)
    if before!=after:raise RuntimeError('Main changes exceed the exact Search origin visibility correction')


def focused_qml_coverage(root, run, base_id, suite):
    if suite=='media_controls':
        # Permit only this scan refusal diagnostic; lifecycle changes still
        # invalidate full coverage and must not hide behind this selector.
        before=manifest(root/'runs'/base_id/'files.sha256');after=manifest(run/'files.sha256')
        for name,old,new in (
            ('src/media/KodiMedia.cpp',
             '    if (m_busy || m_playbackActive || !m_connected) return;\n',
             '    if (m_busy || m_playbackActive || !m_connected) {\n'
             '        m_scanStatus = m_playbackActive ? "Stop playback before scanning, then retry."\n'
             '            : m_busy ? "Kodi is finishing another operation. Wait, then retry the scan."\n'
             '            : "Connect to Kodi before scanning. Refresh the library connection, then retry. " + m_status;\n'
             '        emit changed();\n        return;\n    }\n'),
            ('kodi/service.isolde.bridge/bridge.py',
             '    elif request["method"].startswith("AudioLibrary."):',
             '    elif request["method"] in ("AudioLibrary.GetAlbums", "AudioLibrary.GetSongs"):')):
            if before.get(name)==after.get(name):continue
            original=root/'runs'/base_id/'source'/name;current=run/'source'/name
            if not original.is_file() or not current.is_file() or digest(original)!=before.get(name) or digest(current)!=after.get(name):raise RuntimeError('Exact scan correction sources required')
            old_text=original.read_text();new_text=current.read_text()
            if new_text.count(new)!=1 or new_text.replace(new,old,1)!=old_text:raise RuntimeError('Media correction exceeds the scan-only boundary')
        proof=qml_coverage(root,run,base_id,suite,'media_controls',
            {'src/media/KodiService.h','src/media/KodiService.cpp','src/media/KodiMedia.cpp','config/kodi-capabilities.json','qml/KodiSettingsView.qml','qml/KodiLibraryView.qml','qml/KodiSourcesView.qml','qml/KodiImportView.qml','qml/PlaybackOptionsWindow.qml','tools/kodi-window.py','native/kodi/IsoldeSources.cpp','kodi/service.isolde.bridge/bridge.py','kodi/service.isolde.bridge/artwork.py','kodi/service.isolde.bridge/service.py'},
            {'tests/tst_home_navigation.cpp','tests/tst_kodi_media.cpp','tests/test_kodi_bridge.py','tests/test_vm_oracle.py','tests/test_kodi_artwork.py','tests/test_kodi_playback.py','tools/build-managed-kodi.py','tools/vm-oracle.py','tools/vm-test.ps1','tools/tv-library-acceptance.py','tools/test-all.ps1','tools/run-visual-acceptance.ps1'},'Media',{'docs/vm-storage-policy.md'})
        proof['changedMediaFiles']=proof.pop('changedMediaQml')
        return proof
    if suite=='store':
        proof=qml_coverage(root,run,base_id,suite,'store',{'tools/android-store-transaction.py','tools/fdroid-store.py','tools/android-setup.py','tools/provision-android.py','qml/AppStoreView.qml','src/input/web-remote.js','src/sessions/NativeAppHost.cpp','src/sessions/ChromiumRemotePipe.h'},
            {'tools/backlog-progress.ps1','tests/tst_isolde.cpp','tests/web_remote.cpp','tools/native-session-smoke.cpp','tools/web-remote-acceptance.py','tests/test_fdroid_store.py','tests/test_android_provisioning.py','tests/test_store_lifecycle.py','tests/studio/tst_selection.qml','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/home-components-acceptance.py','tools/store-acceptance.py','tools/test-all.ps1','tools/run-visual-acceptance.ps1'},'Store',{'tools/web-remote-acceptance.py','docs/store-android-authorization-fixture.md'})
        proof['changedStoreFiles']=proof.pop('changedStoreQml')
        return proof
    if suite=='sources':
        return qml_coverage(root,run,base_id,suite,'sources',{'qml/PageSourcesWidget.qml','qml/HomeScreen.qml','qml/Main.qml','qml/SourceSettingsView.qml','qml/PageStudioView.qml','qml/AppGallery.qml'},
            {'tests/studio/quicktest.cpp','tests/studio/tst_editor.qml','tests/studio/tst_sources.qml','tests/tst_home_navigation.cpp','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/vm-process.ps1','tools/test-all.ps1','tools/index-vm-review.ps1','tools/run-visual-acceptance.ps1','tools/home-components-acceptance.py'},'Sources',{'docs/acceptance-reconciliation-2026-11-11.md'})
    if suite=='shell_appearance':
        return qml_coverage(root,run,base_id,suite,'shell_appearance',{'qml/NetworkSettingsView.qml','qml/PageAppCarousel.qml','qml/OrbCard.qml','qml/AudioSettingsView.qml','qml/AmbientSettingsView.qml'},
            {'tests/studio/tst_shell.qml','tests/tst_surface_appearance.cpp','tests/fixtures/shell-theme-routes.json',
             'tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/home-components-acceptance.py','tools/reference_collections.py','tools/run-visual-acceptance.ps1','tools/test-all.ps1'},
            'ShellKeyboard',{'tests/fixtures/shell-theme-routes.json'})
    if suite=='studio_visuals':
        before=manifest(root/'runs'/base_id/'files.sha256');after=manifest(run/'files.sha256')
        if before.get('qml/Main.qml')!=after.get('qml/Main.qml'):
            old=root/'runs'/base_id/'source/qml/Main.qml';new=run/'source/qml/Main.qml'
            if not old.is_file() or not new.is_file() or digest(old)!=before.get('qml/Main.qml') or digest(new)!=after.get('qml/Main.qml'):
                raise RuntimeError('Exact frozen Main sources required for Search origin reuse')
            search_origin_main_delta(old.read_text(),new.read_text())
        icons={'config/app-icons/'+name+'.svg' for name in ('steam','spotify','kodi','freetube','netflix','cbc-gem','crunchyroll','paramount-plus','youtube-tv','prime-video','disney-plus','peacock')}
        return qml_coverage(root,run,base_id,suite,'studio_visuals',
            {'qml/Main.qml','qml/PageStudioView.qml','qml/CommandCenter.qml','qml/HomeCard.qml'}|icons,
            {'config/app-icons/provenance.json','tests/tst_home_navigation.cpp','tests/studio/tst_editor.qml','tests/studio/tst_artwork.qml','tests/studio/tst_shell.qml','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/run-visual-acceptance.ps1','tools/test-all.ps1','tools/home-components-acceptance.py','tools/home-reference-acceptance.py','tools/tv-library-acceptance.py'},'StudioVisuals')
    if suite=='studio_artwork':
        return qml_coverage(root,run,base_id,suite,'studio_artwork',
            {'qml/StudioImageFraming.qml','qml/StudioArtworkPicker.qml','qml/StudioInspector.qml','qml/PageStudioView.qml'},
            {'tests/studio/tst_artwork.qml','tests/studio/tst_colors.qml','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/run-visual-acceptance.ps1','tools/home-components-acceptance.py'},'StudioArtwork')
    if suite=='studio_selection':
        return qml_coverage(root,run,base_id,suite,'studio_selection',
            {'qml/StudioInspector.qml','qml/StudioAppPicker.qml','qml/PageLibraryWidget.qml'},
            {'tests/studio/tst_selection.qml','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/run-visual-acceptance.ps1','tools/tv-library-acceptance.py','tools/home-components-acceptance.py'},'StudioInspector')
    if suite=='home_component_navigation':
        return qml_coverage(root,run,base_id,suite,'home_component_navigation',
            {'qml/ComposedHome.qml','qml/PlacedComponents.qml','qml/AppGallery.qml'},
            {'tests/tst_home_navigation.cpp','tests/test_vm_oracle.py','tools/vm-test.ps1','tools/vm-oracle.py','tools/home-reference-acceptance.py','tools/home-components-acceptance.py','tools/run-visual-acceptance.ps1','tools/test-all.ps1'},'Studio',{'docs/test-campaign-2026-11-09.md'})
    return guide_qml_coverage(root,run,base_id,suite)


def ctest_command(build, report):
    return ["ctest", "--test-dir", str(build), "--output-on-failure", "--stop-on-failure",
            "--timeout", "120", "--output-junit", str(report)]


def retained_storage_runs(root, current, extra=()):
    runs=root/'runs'
    candidates=sorted((p for p in runs.iterdir() if p.is_dir() and not p.is_symlink()),key=lambda p:p.stat().st_mtime,reverse=True)
    staged=sorted((p for p in candidates if (p/'payload').is_dir()),key=lambda p:(p/'payload').stat().st_mtime,reverse=True)
    retained={current,*candidates[:3],*staged[:3],*extra}
    retained.update(p for p in candidates if (p/'guide-soak-receipt.json').exists() or (p/'guide-soak-receipt.json').is_symlink())
    pending=list(retained)
    while pending:
        candidate=pending.pop();record=candidate/'test-result.json'
        if candidate.parent!=runs or candidate.resolve()!=candidate:raise RuntimeError('Unsafe retained candidate')
        if not record.exists():continue
        if record.resolve()!=record or record.stat().st_size>4*1024**2:raise RuntimeError('Unsafe retained receipt')
        proof=json.loads(record.read_text()).get('coverageReuse')
        if not proof:continue
        base=proof.get('baseRunId') if isinstance(proof,dict) else None
        if not isinstance(base,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',base):raise RuntimeError('Unsafe retained coverage base')
        path=runs/base
        if path not in retained:retained.add(path);pending.append(path)
    return retained


def release_storage_identity(release, mounts=()):
    if release.resolve()!=release or not release.is_dir() or not re.fullmatch(r'[a-f0-9]{24}',release.name):raise RuntimeError('Unsafe retired release path')
    if any(m!=Path('/') and (m==release or m.is_relative_to(release) or release.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted retired release refused')
    authored=hashlib.sha256();observed=hashlib.sha256();size=0;files=0;generated=[]
    for path in sorted(release.rglob('*')):
        if path.resolve()!=path or path.is_symlink():raise RuntimeError('Redirected retired release member')
        info=path.stat()
        if info.st_uid!=os.getuid():raise RuntimeError('Foreign-owned retired release member')
        if path.is_dir():continue
        if not path.is_file() or info.st_size>1024**3 or files>=65536:raise RuntimeError('Unbounded retired release member')
        name=path.relative_to(release).as_posix();files+=1;size+=info.st_size
        cache=re.fullmatch(r'([A-Za-z0-9_-]+)\.cpython-[0-9]+(?:\.opt-[12])?\.pyc',path.name)
        is_cache=path.parent.name=='__pycache__' and cache and info.st_size<=4*1024**2 and (path.parent.parent/(cache[1]+'.py')).is_file()
        observed.update(name.encode()+b'\0');observed.update(str(info.st_mode).encode()+b'\0')
        if not is_cache:authored.update(name.encode()+b'\0')
        else:generated.append(name)
        with path.open('rb') as stream:
            for chunk in iter(lambda:stream.read(1024**2),b''):
                observed.update(chunk)
                if not is_cache:authored.update(chunk)
    if not (release/'bin/isolde').is_file():raise RuntimeError('Incomplete retired release')
    return dict(contentId=authored.hexdigest()[:24],treeSha256=observed.hexdigest(),bytes=size,files=files,generatedPythonCache=generated)


def storage_retention_plan(root, current, mounts=(), extra=(), active=()):
    if root.resolve()!=root or (root/'runs').resolve()!=root/'runs':raise RuntimeError('Unsafe storage retention root')
    prefix=root.parent/'.local/share/isolde-appliance';releases=prefix/'releases';retained=retained_storage_runs(root,current,extra)
    protected=set();references=[]
    if releases.exists() and (releases.resolve()!=releases or not releases.is_dir()):raise RuntimeError('Redirected release directory')
    def pin(path,reason):
        if path.parent!=releases or not re.fullmatch(r'[a-f0-9]{24}',path.name) or path.resolve()!=path:raise RuntimeError('Unsafe release retention reference')
        protected.add(path);references.append(dict(release=str(path),reason=reason))
    for name in ('current','previous'):
        link=prefix/name
        if link.exists() or link.is_symlink():
            if not link.is_symlink():raise RuntimeError('Release link is not a symlink')
            pin(link.resolve(strict=True),name)
    installed=sorted((p for p in releases.iterdir() if p.is_dir()),key=lambda p:p.stat().st_mtime,reverse=True) if releases.exists() else []
    for path in installed[:3]:pin(path,'newest three releases')
    for candidate in retained:
        receipt=candidate/'deploy-result.json'
        if receipt.exists():
            if receipt.resolve()!=receipt or receipt.stat().st_size>262144:raise RuntimeError('Unsafe retained deployment receipt')
            path=json.loads(receipt.read_text()).get('release')
            if path:pin(Path(path),'retained candidate '+candidate.name)
    units=root.parent/'.config/systemd/user'
    for unit in list(units.glob('isolde*.service'))[:32]:
        if unit.resolve()!=unit or unit.stat().st_size>65536:raise RuntimeError('Unsafe Isolde service reference')
        for identity in re.findall(re.escape(str(releases))+r'/([a-f0-9]{24})(?:/|["\s])',unit.read_text()):pin(releases/identity,'service '+unit.name)
    for path in active:
        for release in installed:
            if path==release or path.is_relative_to(release):pin(release,'live process')
    plan=dict(policyVersion=1,keepNewestReleases=3,keepNewestPayloads=3,retainedRuns=sorted(p.name for p in retained),protectedReleases=references,releases=[],payloads=[],unverifiedRetained=[],refused=[])
    for release in installed:
        if release in protected:continue
        try:identity=release_storage_identity(release,mounts)
        except RuntimeError as error:
            plan['refused'].append(dict(path=str(release),reason=str(error)));continue
        if identity['contentId']!=release.name:
            plan['refused'].append(dict(path=str(release),reason='release content-address mismatch',observed=identity));continue
        plan['releases'].append(dict(path=str(release),identity=identity))
    for candidate in (root/'runs').iterdir():
        payload=candidate/'payload';receipt=candidate/'test-result.json'
        if candidate in retained or not payload.exists():continue
        if candidate.resolve()!=candidate or payload.resolve()!=payload or receipt.resolve()!=receipt:
            plan['refused'].append(dict(path=str(payload),reason='redirected obsolete staged payload'));continue
        if not receipt.is_file():
            plan['unverifiedRetained'].append(dict(path=str(payload),reason='no passing test receipt'));continue
        if any(path==payload or path.is_relative_to(payload) for path in active):continue
        if any(m!=Path('/') and (m==payload or m.is_relative_to(payload) or payload.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted obsolete staged payload')
        result=json.loads(receipt.read_text())
        if result.get('passed') is not True:
            plan['unverifiedRetained'].append(dict(path=str(payload),reason='no passing test receipt'));continue
        if payload_hashes(payload)!=result.get('payload'):
            plan['refused'].append(dict(path=str(payload),reason='obsolete staged payload identity mismatch'));continue
        plan['payloads'].append(dict(path=str(payload),hashes=result['payload']))
    return plan


def apply_storage_retention(root, plan, mounts=(), active=()):
    if plan.get('policyVersion')!=1 or plan.get('refused'):raise RuntimeError('Storage audit contains refused identities; no artifacts removed')
    releases=root.parent/'.local/share/isolde-appliance/releases';removed=[]
    for item in plan['releases']:
        path=Path(item['path'])
        if path.parent!=releases or release_storage_identity(path,mounts)!=item['identity']:raise RuntimeError('Retired release changed after storage audit')
        if any(p==path or p.is_relative_to(path) for p in active):raise RuntimeError('Retired release became active')
        for name in ('current','previous'):
            link=releases.parent/name
            if link.is_symlink() and link.resolve(strict=True)==path:raise RuntimeError('Retired release became current or previous')
    for item in plan['payloads']:
        path=Path(item['path'])
        if path.name!='payload' or path.parent.parent!=root/'runs' or path.resolve()!=path or payload_hashes(path)!=item['hashes']:raise RuntimeError('Staged payload changed after storage audit')
        if any(m!=Path('/') and (m==path or m.is_relative_to(path) or path.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted staged payload before removal')
        if any(p==path or p.is_relative_to(path) for p in active):raise RuntimeError('Staged payload became active')
    for item in [*plan['releases'],*plan['payloads']]:
        path=Path(item['path'])
        if path.resolve()!=path:raise RuntimeError('Storage target redirected before removal')
        shutil.rmtree(path);removed.append(str(path))
    return removed


def campaign_pending(registered, results):
    if not registered or len(set(registered)) != len(registered):
        raise RuntimeError('Invalid campaign registrations')
    names=[item['name'] for item in results]
    if len(set(names)) != len(names) or not set(names).issubset(registered):
        raise RuntimeError('Unknown or repeated campaign test result')
    return [name for name in registered if name not in names]


def campaign_results(path):
    cases=list(ET.parse(path).getroot().iter('testcase'))
    return [{'name':case.get('name'), 'state':'failed' if case.find('failure') is not None or case.find('error') is not None
             else 'skipped' if case.find('skipped') is not None or case.get('status') in {'notrun','disabled'} else 'passed'} for case in cases]


def diagnostic_campaign_receipt(result, progress):
    if result.get('acceptance') is not False or result.get('passed') is not False or result.get('fullSuite') is not False:
        raise RuntimeError('Diagnostic receipt must retain failed/nonacceptance status')
    if progress.get('acceptance') is not False or progress.get('pending') or campaign_pending(progress['registered'],progress['results']):
        raise RuntimeError('Every registration must be attempted before diagnostic staging')
    if result.get('sourceDigest')!=progress.get('sourceDigest') or result.get('runs')!=progress.get('runs') or not result.get('payload'):
        raise RuntimeError('Diagnostic candidate identity mismatch')
    return True


class Oracle:
    def __init__(self, root, run_id):
        self.root = root
        self.run = root / "runs" / run_id
        self.run.mkdir(parents=True, exist_ok=True)
        self.state = {"runId": run_id, "state": "running", "stage": "starting",
                      "pid": os.getpid(), "started": time.time(), "logs": str(self.run)}
        self.state["driverDigest"] = digest(Path(__file__))
        self.state["processStartTicks"] = Path("/proc/self/stat").read_text().split()[21] if os.name == "posix" else None

    def update(self, **values):
        self.state.update(values, updated=time.time())
        atomic_json(self.run / "status.json", self.state)
        atomic_json(self.root / "latest.json", self.state)

    def storage_retention(self, apply=False):
        if self.command('hostname')!='isolde-test' or self.root!=Path.home()/'isolde-oracle' or self.root.resolve()!=self.root:raise RuntimeError('Storage retention is restricted to Isolde-Test')
        mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
        def live_paths():
            found=[];releases=Path.home()/'.local/share/isolde-appliance/releases'
            for process in Path('/proc').iterdir():
                if not process.name.isdigit():continue
                for name in ('exe','cwd'):
                    try:found.append((process/name).resolve(strict=True))
                    except (OSError,RuntimeError):pass
                try:
                    command=(process/'cmdline').read_bytes()[:262144]
                    for identity in re.findall(re.escape(os.fsencode(releases))+rb'/([a-f0-9]{24})(?:/|\x00)',command):found.append(releases/identity.decode())
                except OSError:pass
            return found
        prefix=Path.home()/'.local/share/isolde-appliance';prefix.mkdir(parents=True,exist_ok=True)
        with exclusive_lock(prefix/'.install.lock'):
            service_before=self.service();links_before=self.links();layouts_before=self.layout_hashes()
            before=shutil.disk_usage(self.root).free
            plan=storage_retention_plan(self.root,self.run,mounts,getattr(self,'maintenance_pins',()),live_paths())
            atomic_json(self.run/'storage-retention-plan.json',plan)
            removed=apply_storage_retention(self.root,plan,mounts,live_paths()) if apply else []
            after=shutil.disk_usage(self.root).free
            result=dict(applied=apply,removed=removed,freeBefore=before,freeAfter=after,reclaimedBytes=after-before,retainedRuns=plan['retainedRuns'],refused=plan['refused'],normalProfilesPreserved=True,incrementalBuildCachePreserved=True)
            result.update(serviceBefore=service_before,serviceAfter=self.service(),linksBefore=links_before,linksAfter=self.links(),normalLayoutsUnchanged=layouts_before==self.layout_hashes())
            atomic_json(self.run/'storage-retention-result.json',result)
            if apply:
                if result['serviceBefore']!=result['serviceAfter'] or result['linksBefore']!=result['linksAfter'] or not result['normalLayoutsUnchanged']:raise RuntimeError('Normal service, releases or layouts changed during storage retention')
                atomic_json(self.root/'storage-policy.json',dict(version=1,keepNewestReleases=3,keepNewestPayloads=3,protectCurrentPreviousRecoveryLiveAndReachableBases=True,activatedBy=self.state['runId']))
            print('STORAGE_RETENTION '+json.dumps(dict(applied=apply,releases=len(plan['releases']),payloads=len(plan['payloads']),refused=len(plan['refused']),freeGiB=round(after/1024**3,2))),flush=True)
            return result

    def stage(self, name, argv, timeout=120, env=None, on_line=None):
        self.update(stage=name)
        print("STAGE " + name, flush=True)
        log = self.run / (name + ".log")
        if log.exists():
            log = self.run / (name + "-" + str(time.time_ns()) + ".log")
        self.update(stageLog=str(log))
        process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   env=env, start_new_session=True, bufsize=0)
        start = last_heartbeat = time.monotonic()
        pending = b""
        tail = ""
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        try:
            with log.open("wb") as output:
                while selector.get_map():
                    now = time.monotonic()
                    if now - start > timeout:
                        raise TimeoutError(f"{name} exceeded {timeout}s; see {log}")
                    if now - last_heartbeat >= 10:
                        self.update(elapsedSeconds=round(now - start), lastOutput=tail[-300:])
                        print(f"PROGRESS {name} {int(now-start)}s {tail[-180:]}", flush=True)
                        last_heartbeat = now
                    for key, _ in selector.select(0.25):
                        data = os.read(key.fd, 65536)
                        if not data:
                            selector.unregister(key.fileobj)
                            continue
                        output.write(data)
                        output.flush()
                        pending += data
                        while b"\n" in pending:
                            line, pending = pending.split(b"\n", 1)
                            tail = line.decode("utf-8", "replace").strip()
                            if on_line:
                                on_line(tail)
                code = process.wait(timeout=5)
                if pending:
                    tail = pending.decode("utf-8", "replace")
                    if on_line:
                        on_line(tail)
            if code:
                lines = log.read_text(errors="replace").splitlines()[-25:]
                # A failed graphical stage already streamed control markers.
                # Prefix diagnostic replay so the host cannot interpret an old
                # *_READY line as a new capture/input checkpoint.
                print("\n".join(("LOG " if on_line else "")+line for line in lines), flush=True)
                raise RuntimeError(f"{name} exited {code}; see {log}")
        finally:
            selector.close()
            stop_owned(process)

    def command(self, *argv):
        return subprocess.check_output(argv, text=True, timeout=15).strip()

    def service(self):
        text = self.command("systemctl", "--user", "show", "isolde.service", "-p", "ActiveState",
                            "-p", "SubState", "-p", "NRestarts", "-p", "MainPID")
        return dict(line.split("=", 1) for line in text.splitlines() if "=" in line)

    def links(self):
        prefix = Path.home() / ".local/share/isolde-appliance"
        return {name: str((prefix / name).resolve()) for name in ("current", "previous")}

    def deployed_release(self, payload, target):
        deployment = self.run / "deploy-result.json"
        if deployment.is_file():
            release = json.loads(deployment.read_text()).get("release")
            if not isinstance(release, str) or self.links()["current"] != release or digest(Path(release) / "bin/isolde") != digest(payload / "bin/isolde"):
                raise RuntimeError("Native configuration requires this deployed snapshot in Isolde-Test")
            return release
        if target != "android":
            raise RuntimeError("Native configuration requires this deployed snapshot in Isolde-Test")
        release = self.links()["current"]
        release_path = Path(release)
        releases = (Path.home() / ".local/share/isolde-appliance/releases").resolve()
        if release_path.parent != releases or not release_path.is_dir():
            raise RuntimeError("Android setup without a deployment receipt requires the current Isolde release")
        if payload_hashes(release_path) != payload_hashes(payload):
            raise RuntimeError("Android setup without a deployment receipt requires an exact match to this tested payload")
        return release

    def restore(self, before):
        expected = before["ActiveState"] == "active"
        active = self.service()["ActiveState"] == "active"
        if active != expected:
            self.command("systemctl", "--user", "start" if expected else "stop", "isolde.service")
        after = self.service()
        if (after["ActiveState"] == "active") != expected:
            raise RuntimeError("Normal service restoration failed")
        return after

    def check(self):
        atomic_json(self.run/"guide-runtime-capabilities.json",guide_backend_probe(Path.home()))
        # T30 capability discovery is bounded. No game startup, account
        # enumeration, ROM scan or runtime installation occurs here.
        game_tools = {name: shutil.which(name) for name in
                      ("PPSSPPSDL", "PPSSPPQt", "ppsspp", "retroarch", "dolphin-emu", "pcsx2-qt", "flatpak", "wine", "wine64", "neverball", "supertux2")}
        package_probe = subprocess.run(
            ["apt-cache", "policy", "ppsspp", "libretro-ppsspp", "retroarch", "libretro-nestopia", "libretro-gambatte", "libretro-mgba", "libretro-bsnes-mercury-balanced", "dolphin-emu", "pcsx2", "neverball", "supertux", "libsdl2-dev", "libqt6sql6-sqlite"],
            capture_output=True, text=True, timeout=15)
        atomic_json(self.run / "games-runtime-capabilities.json", {
            "executables": game_tools, "architecture": os.uname().machine,
            "packagePolicyExit": package_probe.returncode,
            "packagePolicy": package_probe.stdout[:16384], "error": package_probe.stderr[:2048],
            "physicalControllerQualified": False, "physicalOpticalQualified": False})
        game_help = {}
        for binary, argument in (("/usr/games/dolphin-emu", "--help"), ("/usr/games/pcsx2-qt", "-help"), ("/usr/games/supertux2", "--help"), ("/usr/bin/retroarch", "--help")):
            if not Path(binary).is_file():continue
            with tempfile.TemporaryDirectory(prefix="games-help-",dir=self.run) as private:
                environment=dict(os.environ,HOME=private,XDG_CONFIG_HOME=private+"/config",XDG_DATA_HOME=private+"/data",QT_QPA_PLATFORM="offscreen")
                result=subprocess.run([binary,argument],env=environment,capture_output=True,text=True,timeout=15)
                game_help[binary]={"exit":result.returncode,"stdout":result.stdout[:16384],"stderr":result.stderr[:4096]}
        atomic_json(self.run/"games-runtime-help.json",game_help)
        for tool in ("cmake", "ninja", "ctest", "python3", "tar"):
            if not shutil.which(tool):
                raise RuntimeError("Required guest tool missing: " + tool)
        free = shutil.disk_usage(self.root).free
        if free < 2 * 1024**3:
            raise RuntimeError("Less than 2 GiB free after test preflight; inspect guest cleanup receipts and storage.")
        self.update(hostname=self.command("hostname"), freeGiB=round(free / 1024**3, 2),
                    service=self.service(), releases=self.links())
        environment = dict(os.environ)
        for line in self.command("systemctl", "--user", "show-environment").splitlines():
            key, _, value = line.partition("=")
            if key in {"DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR", "DISPLAY", "XAUTHORITY"}: environment[key] = value
        portal = subprocess.run(["gdbus", "introspect", "--session", "--dest", "org.freedesktop.portal.Desktop", "--object-path", "/org/freedesktop/portal/desktop"], env=environment, capture_output=True, text=True, timeout=15)
        app_metadata = {}
        for app_id in ("spotify", "steam", "freetube"):
            entries = {}
            for root in (Path.home()/".local/share", Path("/usr/share")):
                for desktop_id in (app_id, "isolde-"+app_id):
                    entry = root/"applications"/(desktop_id+".desktop")
                    if entry.is_file() and entry.stat().st_size <= 16384: entries[str(entry)] = entry.read_text()[:16384]
            icons = []
            for pattern in ("icons/hicolor/*/apps/"+app_id+"*", "pixmaps/"+app_id+"*", app_id+"/icons/*"):
                icons.extend(str(p) for p in Path("/usr/share").glob(pattern) if p.is_file())
            app_metadata[app_id] = {"entries": entries, "icons": sorted(icons)[:64]}
        atomic_json(self.run / "desktop-platform.json", {"portalExit": portal.returncode, "appMetadata": app_metadata,
            "globalShortcuts": "org.freedesktop.portal.GlobalShortcuts" in portal.stdout,
            "portalInterfaces": portal.stdout[:65536], "error": portal.stderr[:2048],
            "desktopIdentity": (Path.home()/".local/share/applications/org.isolde.Isolde.desktop").is_file()})
        locations = [Path.home()/"isolde-runs", Path.home(), Path("/var/cache/apt"), Path("/var/lib/systemd/coredump")]
        atomic_json(self.run / "guest-disk-usage.json", {"guestHome": str(Path.home()), **guest_disk_usage_diagnostic(locations)})
        privileges = subprocess.run(["sudo", "-n", "-l"], capture_output=True, text=True, timeout=10)
        atomic_json(self.run / "guest-privileges.json", {"exit": privileges.returncode, "policy": privileges.stdout[:8192], "error": privileges.stderr[:2048]})
        atomic_json(self.run/"native-admin-tools.json", {name:shutil.which(name) for name in ("pkexec","gnome-terminal","kgx","ptyxis","systemd-run")})
        terminal_help=subprocess.run(["ptyxis","--help"],env=environment,capture_output=True,text=True,timeout=10)
        atomic_json(self.run/"native-terminal-help.json",{"exit":terminal_help.returncode,"output":terminal_help.stdout[:16384],"error":terminal_help.stderr[:2048]})
        native_setup={}
        for filename in ("/usr/lib/isolde-native/bwrap","/etc/apparmor.d/isolde-native"):
            file=Path(filename)
            native_setup[filename]={"exists":file.is_file(),"symlink":file.is_symlink()}
            if file.is_file(): native_setup[filename].update(uid=file.stat().st_uid,mode=oct(file.stat().st_mode & 0o7777),sha256=digest(file))
        if Path("/usr/lib/isolde-native/bwrap").is_file():
            native_setup["ancestors"]=[{"path":str(p),"symlink":p.is_symlink(),"uid":p.stat().st_uid,"mode":oct(p.stat().st_mode & 0o7777)} for p in Path("/usr/lib/isolde-native/bwrap").parents]
            label=subprocess.run(["/usr/lib/isolde-native/bwrap","--ro-bind","/","/","--proc","/proc","--","/usr/bin/cat","/proc/self/attr/current"],capture_output=True,text=True,timeout=10)
            native_setup["loadedLabel"]={"exit":label.returncode,"output":label.stdout[:2048],"error":label.stderr[:2048]}
        atomic_json(self.run/"native-setup-state.json",native_setup)
        native_home = Path.home()/".local/share/Isolde/native-profiles/8bdf6077-4613-4b52-9b20-d1ec7931dfcf/steam/home"
        native_logs = {}
        if native_home.is_dir() and not native_home.is_symlink():
            for directory, children, files in os.walk(native_home, followlinks=False):
                children[:] = [c for c in children if not (Path(directory)/c).is_symlink()]
                if len(Path(directory).relative_to(native_home).parts)>7: children[:]=[]; continue
                for name in files:
                    file = Path(directory)/name
                    if name in {"bootstrap_log.txt", "console-linux.txt", "steam-runtime-check-requirements.log"} and not file.is_symlink() and file.stat().st_size <= 4*1024*1024:
                        native_logs[str(file.relative_to(native_home))] = file.read_text(errors="replace")[-16384:]
                if len(native_logs)>=8: break
        atomic_json(self.run/"native-bootstrap-diagnostics.json", native_logs)
        namespace_policy = {}
        for filename in ("/proc/sys/kernel/unprivileged_userns_clone", "/proc/sys/kernel/apparmor_restrict_unprivileged_userns", "/proc/sys/user/max_user_namespaces", "/etc/apparmor.d/bwrap", "/etc/apparmor.d/bubblewrap", "/etc/apparmor.d/steam", "/proc/self/attr/current"):
            file = Path(filename)
            try: namespace_policy[filename] = file.read_text()[:16384]
            except OSError as error: namespace_policy[filename] = str(error)
        atomic_json(self.run/"native-namespace-policy.json", namespace_policy)
        denials = subprocess.run(["journalctl", "-k", "--since=-20min", "--no-pager", "-g", "userns|apparmor"], capture_output=True, text=True, timeout=15)
        atomic_json(self.run/"native-namespace-denials.json", {"exit": denials.returncode, "output": denials.stdout[-16384:], "error": denials.stderr[:2048]})
        matching_profiles = {}
        for file in sorted(Path("/etc/apparmor.d").glob("*"))[:256]:
            if file.is_file() and not file.is_symlink() and file.stat().st_size<=65536:
                content=file.read_text(errors="replace")
                if "unpriv_bwrap" in content: matching_profiles[file.name]=content
        atomic_json(self.run/"native-bwrap-profile.json", matching_profiles)
        # Read-only Android runtime preflight. Never load modules, install a
        # runtime, change graphics or inspect Android/account data here.
        kernel = os.uname().release
        android = {"kernel": kernel, "machine": os.uname().machine,
                   "tools": {name: shutil.which(name) for name in ("waydroid", "weston", "adb")},
                   "binderDevices": [str(p) for p in (Path("/dev/binder"), Path("/dev/hwbinder"), Path("/dev/vndbinder"), Path("/dev/binderfs")) if p.exists()]}
        config = Path("/boot") / ("config-" + kernel)
        android["kernelConfig"] = [line for line in config.read_text().splitlines() if "ANDROID_BINDER" in line or "MEMFD_CREATE=" in line] if config.is_file() and config.stat().st_size < 1024*1024 else []
        android["binderFilesystemAvailable"] = "binder" in Path("/proc/filesystems").read_text()
        for label, command in (("binderModule", ["modinfo", "binder_linux"]),
                               ("packages", ["apt-cache", "policy", "waydroid", "weston"]),
                               ("runtimeService", ["systemctl", "show", "waydroid-container.service", "-p", "LoadState", "-p", "ActiveState"]),
                               ("graphics", ["lspci", "-nn"])):
            if not shutil.which(command[0]):
                android[label] = {"available": False}
                continue
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            output = result.stdout
            if label == "graphics": output = "\n".join(line for line in output.splitlines() if any(word in line.lower() for word in ("vga", "display", "3d controller")))
            android[label] = {"exit": result.returncode, "output": output[:8192], "error": result.stderr[:2048]}
        android["session"] = {key: value for line in self.command("systemctl", "--user", "show-environment").splitlines()
                              for key, separator, value in [line.partition("=")] if separator and key in {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "WAYLAND_DISPLAY", "DISPLAY"}}
        release = Path(self.state["releases"]["current"])
        config_path = release / "share/isolde/config/android-runtime.json"
        if release.is_symlink() or not config_path.is_file() or config_path.is_symlink() or config_path.stat().st_size > 65536:
            raise RuntimeError("Cannot safely inspect the current release's Android repository configuration")
        runtime_config = json.loads(config_path.read_text())
        key_url = runtime_config["repository"]["keyUrl"]
        parsed_key_url = urlsplit(key_url)
        if parsed_key_url.scheme != "https" or not parsed_key_url.hostname:
            raise RuntimeError("Current Android repository key URL is invalid")
        android["repositoryDns"] = repository_dns_probe(parsed_key_url.hostname)
        android["resolverStatus"] = resolver_status()
        atomic_json(self.run/"android-capabilities.json", android)

    def test(self, suite, reuse_full_run=None, resume_run_id=None):
        if reuse_full_run and not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', reuse_full_run):
            raise RuntimeError("Unsafe full-run reuse identity")
        self.maintenance_pins = {self.root / 'runs' / reuse_full_run} if reuse_full_run else set()
        source, entries = extract_snapshot(self.run)
        self.stage("oracle-self-test", ["python3", str(source / "tests/test_vm_oracle.py")], 90)
        if (self.root/'storage-policy.json').is_file():self.storage_retention(apply=True)
        if resume_run_id:
            self.resume_tests(entries, resume_run_id)
            return
        selected_targets=SUITES[suite]
        self.maintenance_build_targets=selected_targets if isinstance(selected_targets,tuple) else (selected_targets,) if selected_targets else ()
        self.automatic_test_cleanup()
        self.check()
        service_before, releases_before = self.state["service"], self.state["releases"]
        reuse=focused_qml_coverage(self.root,self.run,reuse_full_run,suite) if reuse_full_run else None
        work = self.root / "work"
        old = json.loads((work / "manifest.json").read_text()) if (work / "manifest.json").exists() else {}
        changed = sync_workspace(source, work / "source", entries, old)
        atomic_json(work / "manifest.json", entries)
        self.update(changedFiles=changed, suite=suite, sourceDigest=digest(self.run / "files.sha256"))
        build = work / "build"
        self.stage("configure", ["cmake", "-S", str(work / "source"), "-B", str(build), "-G", "Ninja",
                                 "-DBUILD_TESTING=ON", "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_FLAGS_DEBUG=-g1"], 120)
        command = ["cmake", "--build", str(build), "--parallel", "2"]
        targets, pattern = suite_selection(suite)
        if reuse:targets=targets+['isolde','isolde-recovery','isolde-native-host']
        if reuse and suite=='studio_artwork':pattern='^(studio_artwork|studio_selection|studio_quick|studio_colors|studio_color_interactions)$'
        if targets:
            command += ["--target"] + targets
        self.stage("build", command, 1200)
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                   ISOLDE_TEST_ARTIFACTS=str(self.run))
        command = ctest_command(build, self.run / "ctest.xml")
        if suite != "all":
            command += ["-R", pattern]
        # The 45-suite passing baseline took 557.69s; the 53-suite Studio
        # candidate adds measured design/reuse/cutover/safety coverage. Bound
        # the aggregate at 15 minutes, retaining every individual deadline.
        try:
            self.stage("ctest", command, 900 if suite == "all" else 600 if suite == "studio" else 360, env)
        finally:
            last = build / "Testing/Temporary/LastTest.log"
            if last.is_file():
                shutil.copy2(last, self.run / "LastTest.log")
        xml = ET.parse(self.run / "ctest.xml").getroot()
        cases = list(xml.iter("testcase"))
        if suite != "all":
            expected = {"studio_quick", "studio_safety", "studio_selection", "studio_artwork", "studio_colors", "studio_color_interactions", "studio_workflows", "studio_cutover", "studio_performance", "studio_reuse", "studio_designs", "studio_design_contracts", "studio_gestures", "studio_grid", "studio_grid_controls", "studio_destinations", "studio_ordered", "studio_policies", "studio_overlays", "studio_unit", "studio_behavior"} if suite == "studio" else {suite}
            if reuse and suite == "studio_artwork":expected={"studio_artwork","studio_selection","studio_quick","studio_colors","studio_color_interactions"}
            if suite == "studio_closure_regressions":expected={"studio_quick","studio_colors","studio_color_interactions","home_component_navigation","studio_design_contracts","studio_performance","studio_cutover"}
            if suite == "studio_colors":expected={"studio_colors","studio_color_interactions"}
            if suite == "remote_bindings":expected={"remote_bindings","studio_shell"}
            if suite == "shell_appearance":expected={"studio_selection","studio_shell","surface_appearance_contracts","home_component_navigation"}
            if suite == "campaign_repairs":expected=REPAIR_TESTS
            if suite == "runtime_contracts":expected=RUNTIME_TESTS
            if suite == "studio_visuals":expected={"studio_quick","studio_artwork","studio_shell","home_component_navigation","studio_navigation"}
            if suite == "app_icons":expected={"isolde_tests","component_contracts","studio_artwork","studio_selection","home_component_navigation","studio_navigation"}
            if suite == "studio_remaining":expected={"studio_quick","studio_performance","studio_design_contracts","component_contracts"}
            if suite == "studio_performance":expected={"studio_performance","studio_design_contracts","component_contracts"}
            if suite == "studio_reuse":expected={"studio_reuse","studio_design_contracts"}
            if suite == "studio_designs":
                expected = {"studio_designs", "studio_design_contracts", "component_contracts", "studio_quick", "studio_colors", "studio_color_interactions", "studio_cutover"}
            if suite == "studio_grid":
                expected = {"studio_grid", "studio_grid_controls"}
            if suite == "home_component_navigation":
                expected = {"home_component_navigation", "studio_navigation"}
            if suite == "vpn":
                expected = {"vpn_setup", "vpn_launch"}
            if suite == "sources":
                expected = {"studio_quick", "isolde_tests", "component_contracts", "sources_remote_ui", "home_component_navigation"}
            if suite == "store":
                expected = {"isolde_tests", "studio_selection", "android_provisioning", "web_remote", "fdroid_store", "store_lifecycle"}
            if suite == "media_controls":
                expected = {"home_component_navigation","studio_navigation","kodi_media_client","kodi_local_bridge","kodi_cached_artwork","kodi_playback","kodi_import_contracts","native_profile_isolation"}
            if suite == "games":
                expected = {"component_contracts", "games_remote_ui", "isolde_tests", "games_provisioning", "steam_games"}
            if suite == "guide":
                expected = {"guide_contracts", "guide_remote_ui", "kodi_local_bridge", "kodi_playback", "kodi_media_client"}
            if suite == "kodi_media_client":
                expected = {"kodi_media_client","kodi_import_contracts","kodi_local_bridge"}
            if {c.get("name") for c in cases} != expected:
                raise RuntimeError("CTest did not execute exactly the selected suites")
        if not cases or any(c.find("failure") is not None or c.find("error") is not None for c in cases):
            raise RuntimeError("CTest report contains no tests or failures")
        if any(c.find("skipped") is not None for c in cases):
            raise RuntimeError("CTest skipped tests; inspect the report before claiming a pass")
        verify_tree(work / "source", entries)
        service_after, releases_after = self.service(), self.links()
        self.update(service=service_after, releases=releases_after)
        if service_after != service_before or releases_after != releases_before:
            raise RuntimeError("Installed service or release changed during offscreen Test; inspect evidence, do not deploy")
        result = {"passed": True, "fullSuite": suite == "all", "suite": suite,
                  "tests": len(cases), "sourceDigest": digest(self.run / "files.sha256"),
                  "serviceBefore": service_before, "serviceAfter": service_after,
                  "releasesBefore": releases_before, "releasesAfter": releases_after}
        if reuse:result['coverageReuse']=reuse
        gallery=self.run/'studio-gallery-presentation.json'
        if suite=='sources' and not gallery.is_file():raise RuntimeError('Missing production Studio gallery edit/apply evidence')
        if gallery.is_file():
            if gallery.resolve()!=gallery or gallery.stat().st_size>65536:raise RuntimeError('Unsafe native gallery presentation evidence')
            result['galleryPresentationEvidence']={'file':gallery.name,'sha256':digest(gallery)}
            verified_gallery_evidence(self.run,result)
        if suite == "all" or reuse:
            self.stage("stage-payload", ["cmake", "--install", str(build), "--prefix", str(self.run / "payload")], 120)
            result["payload"] = payload_hashes(self.run / "payload")
        atomic_json(self.run / "test-result.json", result)
        self.update(tests=len(cases), fullSuite=result["fullSuite"])

    def resume_tests(self, entries, previous_id):
        """Diagnostic continuation only: retained failures can never earn a full pass."""
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', previous_id) or previous_id==self.run.name:
            raise RuntimeError('Invalid campaign base')
        previous=self.root/'runs'/previous_id
        prior=manifest(previous/'files.sha256')
        harness={'tools/vm-oracle.py','tools/vm-test.ps1','tools/test-all.ps1','tools/run-visual-acceptance.ps1','tools/tv-library-acceptance.py','tests/test_vm_oracle.py'}
        changed={name for name in set(entries)|set(prior) if entries.get(name)!=prior.get(name)}
        if any(name not in harness and not name.startswith('docs/') for name in changed):
            raise RuntimeError('Campaign application/test bytes changed; a new build is required')
        progress_path=previous/'campaign-progress.json'
        if progress_path.is_file():
            progress=json.loads(progress_path.read_text())
            chain=progress['runs'];results=progress['results']
            origin=self.root/'runs'/chain[0]
        else:
            state=json.loads((previous/'status.json').read_text())
            if state.get('suite')!='all' or state.get('stage')!='ctest' or state.get('state')!='failed':
                raise RuntimeError('Campaign base must be a diagnosed failed full CTest run')
            chain=[previous_id];results=campaign_results(previous/'ctest.xml');origin=previous
        if len(chain)>128 or previous_id!=chain[-1] or self.run.name in chain or any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', item) for item in chain):
            raise RuntimeError('Invalid campaign chain')
        original=manifest(origin/'files.sha256');work=self.root/'work';build=work/'build'
        self.maintenance_pins={origin,previous}
        if shutil.disk_usage(self.root).free < 2*1024**3:
            self.automatic_test_cleanup()
        if shutil.disk_usage(self.root).free < 2*1024**3:
            raise RuntimeError('Campaign continuation requires 2 GiB guest free space')
        verify_tree(work/'source',original)
        if json.loads((work/'manifest.json').read_text())!=original:
            raise RuntimeError('Incremental cache no longer belongs to the campaign candidate')
        inventory=subprocess.run(['ctest','--test-dir',str(build),'--show-only=json-v1'],capture_output=True,text=True,timeout=30,check=True)
        registered=[item['name'] for item in json.loads(inventory.stdout)['tests']]
        pending=campaign_pending(registered,results)
        if not pending:raise RuntimeError('Campaign already attempted every registration; inspect retained failures')
        before_service,before_links=self.service(),self.links()
        self.update(suite='campaign-diagnostic',sourceDigest=digest(origin/'files.sha256'),campaignOrigin=chain[0])
        command=ctest_command(build,self.run/'ctest.xml')+['-R','^('+'|'.join(re.escape(name) for name in pending)+')$']
        try:
            self.stage('ctest',command,900,dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',ISOLDE_TEST_ARTIFACTS=str(self.run)))
        finally:
            if (self.run/'ctest.xml').is_file():results+=campaign_results(self.run/'ctest.xml')
            remaining=campaign_pending(registered,results)
            verify_tree(work/'source',original)
            service,links=self.service(),self.links()
            if service!=before_service or links!=before_links:raise RuntimeError('Installed service/release changed during campaign continuation')
            last=build/'Testing/Temporary/LastTest.log'
            if last.is_file():shutil.copy2(last,self.run/'LastTest.log')
            atomic_json(self.run/'campaign-progress.json',{'acceptance':False,'fullSuite':False,'runs':chain+[self.run.name],
                'sourceDigest':digest(origin/'files.sha256'),'registered':registered,'results':results,'pending':remaining,
                'serviceBefore':before_service,'serviceAfter':service,'releasesBefore':before_links,'releasesAfter':links})
            if not remaining:
                self.stage('stage-diagnostic-payload',['cmake','--install',str(build),'--prefix',str(self.run/'payload')],120)
                atomic_json(self.run/'diagnostic-payload.json',{'acceptance':False,'passed':False,'fullSuite':False,
                    'sourceDigest':digest(origin/'files.sha256'),'runs':chain+[self.run.name],'payload':payload_hashes(self.run/'payload')})
        raise RuntimeError('Diagnostic campaign retained failures; inspect campaign-progress.json. No acceptance payload was produced.')

    def tested_payload(self, diagnostic=False):
        result = json.loads((self.run / ('diagnostic-payload.json' if diagnostic else 'test-result.json')).read_text())
        if diagnostic:
            diagnostic_campaign_receipt(result,json.loads((self.run/'campaign-progress.json').read_text()))
            if payload_hashes(self.run/'payload')!=result['payload']:raise RuntimeError('Diagnostic payload changed')
            verify_tree(self.run/'source',manifest(self.run/'files.sha256'))
            return self.run/'payload'
        if not result.get("passed") or (not result.get("fullSuite") and not result.get('coverageReuse')):
            raise RuntimeError("This run has no passing full-suite payload; run Test -Suite all")
        if not result.get('fullSuite'):
            proof=result['coverageReuse']
            if proof!=focused_qml_coverage(self.root,self.run,proof.get('baseRunId'),result.get('suite')):
                raise RuntimeError('Guide QML coverage proof changed')
        actual=payload_hashes(self.run / "payload")
        if actual != result["payload"]:
            expected=result['payload']
            mismatch={'added':sorted(set(actual)-set(expected))[:20],'missing':sorted(set(expected)-set(actual))[:20],
                      'changed':sorted(name for name in set(actual)&set(expected) if actual[name]!=expected[name])[:20]}
            raise RuntimeError("Staged payload changed after testing; refuse use: "+json.dumps(mismatch))
        verify_tree(self.run / "source", manifest(self.run / "files.sha256"))
        return self.run / "payload"

    def pause_test_indexer(self):
        if self.command('hostname')!='isolde-test' or self.root!=Path.home()/'isolde-oracle' or self.root.resolve()!=self.root:raise RuntimeError('Indexer cache maintenance is guest-only')
        lease=self.root/'test-indexer-lease.json'
        if lease.exists() or lease.is_symlink():raise RuntimeError('Recover the previous test indexer lease before another test')
        units=[]
        for line in self.command('systemctl','--user','list-units','--all','--plain','--no-legend').splitlines():
            fields=line.split()
            if len(fields)>=4 and fields[2]=='active' and re.fullmatch(r'(?:tracker-[A-Za-z0-9_.@-]+|localsearch-3|tinysparql-[A-Za-z0-9_.@-]+)\.service',fields[0]):units.append(fields[0])
        if units:
            atomic_json(lease,dict(guest='isolde-test',runId=self.state['runId'],units=units))
            self.stage('pause-test-indexer',['systemctl','--user','stop',*units],30)
        cache=Path.home()/'.cache/tracker3';reclaimed=0
        if cache.exists() or cache.is_symlink():
            mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
            reclaimed=remove_guest_index_cache(cache,Path.home(),mounts)
        atomic_json(self.run/'test-indexer-cache-cleanup.json',dict(guest='isolde-test',units=units,reclaimedBytes=reclaimed,applicationDataPreserved=True))

    def restore_test_indexer(self):
        lease=self.root/'test-indexer-lease.json'
        if not lease.exists() and not lease.is_symlink():return
        if lease.resolve()!=lease or lease.stat().st_uid!=os.getuid() or lease.stat().st_size>8192:raise RuntimeError('Unsafe test indexer restoration lease')
        record=json.loads(lease.read_text());units=record.get('units',[])
        if record.get('guest')!='isolde-test' or record.get('runId')!=self.state['runId'] or not isinstance(units,list) or len(units)>32 or any(not isinstance(unit,str) or not re.fullmatch(r'(?:tracker-[A-Za-z0-9_.@-]+|localsearch-3|tinysparql-[A-Za-z0-9_.@-]+)\.service',unit) for unit in units):raise RuntimeError('Invalid test indexer restoration lease')
        prior_stage=self.state.get('stage')
        if units:self.stage('restore-test-indexer',['systemctl','--user','start',*units],30)
        lease.unlink();atomic_json(self.run/'test-indexer-restored.json',dict(guest='isolde-test',units=units,restored=True));self.update(stage=prior_stage)

    def restore_kodi_indexer(self):
        lease=self.root/'kodi-indexer-lease.json'
        if not lease.exists() and not lease.is_symlink():return
        if lease.resolve()!=lease or lease.stat().st_size>16384:raise RuntimeError('Unsafe native indexer restoration lease')
        record=json.loads(lease.read_text());units=record.get('units',[])
        if record.get('guest')!='isolde-test' or not isinstance(units,list) or len(units)>32 or any(not isinstance(unit,str) or not re.fullmatch(r'(?:tracker-[A-Za-z0-9_.@-]+|localsearch-3|tinysparql-[A-Za-z0-9_.@-]+)\.service',unit) for unit in units):raise RuntimeError('Invalid native indexer restoration lease')
        if units:self.stage('restore-guest-indexer',['systemctl','--user','start',*units],30)
        atomic_json(self.run/'kodi-indexer-restored.json',dict(guest='isolde-test',units=units,restored=True));lease.unlink()

    def exclude_test_trees_from_indexer(self):
        schemas=set(self.command('gsettings','list-schemas').splitlines())
        schema=next((name for name in ('org.freedesktop.Tracker3.Miner.Files','org.freedesktop.LocalSearch3.Miner.Files') if name in schemas),None)
        policy=self.command('gsettings','get',schema,'ignored-directories-with-content') if schema else ''
        result=dict(schema=schema,configured=False,markers=[],globalSettingsChanged=False)
        if schema and re.search(r"['\"]\.trackerignore['\"]",policy):
            mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
            for directory in (self.root,Path.home()/'Videos/Isolde-Acceptance'):
                if directory.resolve()!=directory or not directory.is_dir() or directory.stat().st_uid!=os.getuid():raise RuntimeError('Unsafe test-tree exclusion root')
                if any(m!=Path('/') and (m==directory or m.is_relative_to(directory) or directory.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted test-tree exclusion root')
                marker=directory/'.trackerignore'
                if marker.exists() or marker.is_symlink():
                    if marker.resolve()!=marker or not marker.is_file() or marker.stat().st_uid!=os.getuid():raise RuntimeError('Unsafe test-tree exclusion marker')
                else:atomic_json(marker,dict(owner='Isolde VM runner',purpose='Disposable test data, excluded from desktop indexing'))
                result['markers'].append(str(marker))
            result['configured']=True
        atomic_json(self.run/'test-tree-indexer-exclusions.json',result)
        return result

    def automatic_test_cleanup(self):
        before = shutil.disk_usage(self.root).free
        if before >= 3 * 1024**3:
            return
        print("PREFLIGHT reclaiming verified guest artifacts (target 3 GiB, minimum 2 GiB)", flush=True)
        if self.state.get('action')=='test':self.pause_test_indexer()
        try:
            self.maintain_artifacts(minimum_gib=2)
        finally:
            after = shutil.disk_usage(self.root).free
            atomic_json(self.run / "guest-test-cleanup.json", {
                "guest": "isolde-test", "freeBefore": before, "freeAfter": after,
                "reclaimedBytes": after - before, "targetGiB": 3, "minimumGiB": 2,
                "targetReached": after >= 3 * 1024**3, "minimumMet": after >= 2 * 1024**3})
        print(f"PREFLIGHT guest cleanup complete: {after / 1024**3:.2f} GiB free", flush=True)

    def maintain_artifacts(self, minimum_gib=3):
        self.prepare_guest_space(3, prune_sources=True, prune_payloads=True)
        backups=self.run/'host-source-backups.json'
        if backups.exists():
            if backups.resolve()!=backups or backups.stat().st_size>262144:raise RuntimeError('Unsafe archive backup receipt')
            archive_mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
            removed=remove_backed_up_archives(self.root,json.loads(backups.read_text()),archive_mounts, preserve_recent=True, target_free=3*1024**3, protected=getattr(self, 'maintenance_pins', ()))
            atomic_json(self.run/'guest-archive-cleanup.json',{'guest':'isolde-test','targets':removed,'hostBackupsVerified':True})
        if shutil.disk_usage(self.root).free<3*1024**3:
            for line in Path('/proc/self/mountinfo').read_text().splitlines():
                if line.split()[4]=='/tmp' and line.split(' - ',1)[1].split()[0] not in {'tmpfs','ext4','xfs','btrfs'}:raise RuntimeError('Acceptance cache cleanup requires native guest /tmp')
            mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
            active=[]
            for process in Path('/proc').iterdir():
                if not process.name.isdigit():continue
                for name in ('cwd','exe'):
                    try:active.append((process/name).resolve(strict=True))
                    except (OSError,RuntimeError):pass
                try:
                    for item in (process/'environ').read_bytes().split(b'\0'):
                        if item.startswith(b'XDG_CACHE_HOME='):active.append(Path(os.fsdecode(item.split(b'=',1)[1])))
                except OSError:pass
            removed=remove_fixture_caches(Path('/tmp'),mounts,active)
            if shutil.disk_usage(self.root).free<2*1024**3:
                # Recent completed caches are reproducible too; active
                # fixtures and all config/media/evidence remain protected.
                removed+=remove_fixture_caches(Path('/tmp'),mounts,active,preserve_recent=0)
            atomic_json(self.run/'guest-fixture-cache-cleanup.json',{'guest':'isolde-test','targets':removed,'fixtureDataPreserved':True,'freeGiB':round(shutil.disk_usage(self.root).free/1024**3,2)})
            if shutil.disk_usage(self.root).free<3*1024**3:
                records=deduplicate_fixture_media(Path.home()/'Videos/Isolde-Acceptance',mounts,active)
                atomic_json(self.run/'guest-fixture-media-dedup.json',{'guest':'isolde-test','files':records,'allPathsAndBytesPreserved':True})
        if shutil.disk_usage(self.root).free<3*1024**3:
            removed=remove_runner_test_bundles(self.root,Path(__file__),mounts)
            atomic_json(self.run/'guest-runner-cache-cleanup.json',{'guest':'isolde-test','targets':removed,'sourceEvidencePreserved':True})
        if shutil.disk_usage(self.root).free<2*1024**3:
            removed=remove_linked_test_executables(self.root,mounts,active,getattr(self,'maintenance_build_targets',()))
            atomic_json(self.run/'guest-linked-tests-cleanup.json',{'guest':'isolde-test','targets':removed,'compiledObjectsPreserved':True})
        if shutil.disk_usage(self.root).free<3*1024**3:
            records=deduplicate_tested_payloads(self.root,mounts)
            atomic_json(self.run/'guest-payload-dedup.json',{'guest':'isolde-test','files':records,'allPathsAndBytesPreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            reports=self.run/'host-report-backups.json'
            if reports.exists():
                if reports.resolve()!=reports or reports.stat().st_size>262144:raise RuntimeError('Unsafe report backup receipt')
                removed=remove_backed_up_reports(self.root,json.loads(reports.read_text()),mounts,protected={self.run,*getattr(self,'maintenance_pins',())})
                atomic_json(self.run/'guest-report-cleanup.json',{'guest':'isolde-test','targets':removed,'hostBackupsVerified':True,'rawEvidencePreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            # Keeping selected linked tests is only a speed optimization. When
            # it prevents preflight, relink them from retained compiled objects.
            removed=remove_linked_test_executables(self.root,mounts,active)
            record=self.run/'guest-linked-tests-cleanup.json'
            prior=json.loads(record.read_text()).get('targets',[]) if record.exists() else []
            atomic_json(record,{'guest':'isolde-test','targets':prior+removed,'compiledObjectsPreserved':True,'selectedTargetsRelinked':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            reports=self.run/'host-report-backups.json'
            if reports.exists():
                if reports.resolve()!=reports or reports.stat().st_size>262144:raise RuntimeError('Unsafe report backup receipt')
                # Only duplicate outer transports: pinned source, payload,
                # logs and case archives remain, with exact host backups.
                removed=remove_backed_up_reports(self.root,json.loads(reports.read_text()),mounts,protected={self.run},target_free=minimum_gib*1024**3,preserve_recent=False)
                record=self.run/'guest-report-cleanup.json'
                prior=json.loads(record.read_text()).get('targets',[]) if record.exists() else []
                atomic_json(record,{'guest':'isolde-test','targets':prior+removed,'hostBackupsVerified':True,'rawEvidencePreserved':True,'pinnedSourceAndPayloadPreserved':True,'recentTransportsPruned':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            removed=remove_failed_source_copies(self.root,self.run,mounts,active,getattr(self,'maintenance_pins',()),minimum_gib*1024**3)
            atomic_json(self.run/'guest-failed-source-cleanup.json',{'guest':'isolde-test','targets':removed,'verifiedRecoveryArchivesPreserved':True,'payloadsAndEvidencePreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            records=deduplicate_source_snapshots(self.root,self.run,mounts,active)
            atomic_json(self.run/'guest-source-dedup.json',{'guest':'isolde-test','files':records,'allSourcePathsAndBytesPreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            removed=remove_unselected_test_objects(self.root,mounts,active,getattr(self,'maintenance_build_targets',()))
            atomic_json(self.run/'guest-test-object-cleanup.json',{'guest':'isolde-test','targets':removed,'productionAndSelectedTestObjectsPreserved':True,'evidencePreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            backups=self.run/'host-source-backups.json'
            if backups.exists():
                if backups.resolve()!=backups or backups.stat().st_size>262144:raise RuntimeError('Unsafe source backup receipt')
                removed=remove_backed_up_archives(self.root,json.loads(backups.read_text()),mounts,target_free=minimum_gib*1024**3,protected={self.run,*getattr(self,'maintenance_pins',())},allow_source_copy=True)
                atomic_json(self.run/'guest-source-transport-cleanup.json',{'guest':'isolde-test','targets':removed,'verifiedHostArchivesAndGuestSourceTreesPreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            removed=remove_packaged_psp_build(Path.home()/'.cache/isolde-emulators/ppsspp-1.20.4',mounts,active,prune_source=True)
            atomic_json(self.run/'guest-packaged-runtime-build-cleanup.json',{'guest':'isolde-test','targets':removed,'verifiedOfflinePackageAndReceiptPreserved':True,'upstreamCheckoutRemoved':any(Path(p).name=='source' for p in removed)})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            # Last resort: regenerate a bounded amount of declared production
            # object cache. Retain source, selected test objects,
            # tests, staged/installed candidates and every evidence path.
            removed=remove_unselected_test_objects(self.root,mounts,active,getattr(self,'maintenance_build_targets',()),target_free=minimum_gib*1024**3,include_production=True)
            atomic_json(self.run/'guest-production-object-cleanup.json',{'guest':'isolde-test','targets':removed,'objectsRegeneratedByNinja':True,'sourceAndAcceptanceEvidencePreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            removed=remove_managed_kodi_archives(self.root,mounts,active)
            atomic_json(self.run/'guest-managed-kodi-cache-cleanup.json',{'guest':'isolde-test','targets':removed,'compiledObjectsAndInstalledDependencyPreserved':True})
        if shutil.disk_usage(self.root).free<minimum_gib*1024**3:
            raise RuntimeError(f"Verified obsolete artifacts could not recover {minimum_gib} GiB; inspect storage evidence")

    def prepare_visual_desktop(self):
        if self.command('hostname')!='isolde-test' or self.root!=Path.home()/'isolde-oracle' or self.root.resolve()!=self.root:
            raise RuntimeError('Desktop preparation is restricted to the native Isolde-Test guest')
        environment=dict(os.environ)
        for line in self.command('systemctl','--user','show-environment').splitlines():
            key,_,value=line.partition('=')
            if key in {'DBUS_SESSION_BUS_ADDRESS','XDG_RUNTIME_DIR','DISPLAY','XAUTHORITY'}:environment[key]=value
        def run(argv):
            return subprocess.run(argv,env=environment,capture_output=True,text=True,timeout=10,check=True).stdout.strip()
        # Cancel outstanding unlock requests, never unlock or read credentials.
        tree=run(['busctl','--user','--list','tree','org.freedesktop.secrets'])
        paths=secret_prompt_paths(tree)
        restart=False
        for path in paths:
            try:
                run(['gdbus','call','--session','--dest','org.freedesktop.secrets','--object-path',path,'--method','org.freedesktop.Secret.Prompt.Dismiss'])
            except subprocess.CalledProcessError as error:
                # Secret Service can restrict Dismiss to its original caller.
                # Restart only this disposable guest's user keyring service to
                # cancel pending operations, without touching credential files.
                if error.stderr is None:raise
                atomic_json(self.run/'guest-prompt-dismiss-error.json',{'error':error.stderr[:2048]})
                run(['systemctl','--user','restart','gnome-keyring-daemon.service'])
                restart=True
                break
        before=run(['gsettings','get','org.gnome.desktop.notifications','show-banners'])
        # Guest desktop notifications must not cover owned-window evidence.
        # This does not change Isolde notification, permission or storage policy.
        run(['gsettings','set','org.gnome.desktop.notifications','show-banners','false'])
        disk_banner=dismiss_guest_disk_banner()
        if disk_banner['identifiedDiskWarning'] and not disk_banner['ignored']:
            raise RuntimeError('Guest disk notification remains; refuse obscured visual evidence')
        if secret_prompt_paths(run(['busctl','--user','--list','tree','org.freedesktop.secrets'])):
            raise RuntimeError('Guest keyring prompts remain; refuse obscured visual evidence')
        atomic_json(self.run/'guest-visual-desktop.json',{'guest':'isolde-test','dismissedPromptPaths':paths,'keyringServiceRestarted':restart,'diskBanner':disk_banner,'desktopBannersBefore':before,'desktopBannersAfter':False,'credentialsRead':False,'credentialsUnlocked':False})

    def configure_native(self, target="native", maintenance_tests_digest=None, demo_art_helper_digest=None, store_helper_digest=None, apply_storage_retention=False):
        if target not in {"native","surfshark","android","android-repair","android-store-support","games","guide","audio-test-tools","browser-test-tools","audio-test-recover","artifacts","desktop-prompts","storage-capacity","kodi-demo-art","store-test-data","kodi-managed","kodi-managed-audit"}: raise ValueError("Unsupported guest configuration target")
        if target in ('kodi-managed','kodi-managed-audit'):
            if self.command('hostname')!='isolde-test':raise RuntimeError('Managed Kodi builds are guest-only')
            source,entries=extract_snapshot(self.run)
            self.stage('oracle-self-test',['python3',str(source/'tests/test_vm_oracle.py')],90)
            if target=='kodi-managed-audit':
                cache=self.root/'support/managed-kodi/distribution-source'
                trees=list(cache.glob('kodi-21.*'))
                if len(trees)!=1 or trees[0].resolve()!=trees[0]:raise RuntimeError('One ordinary authenticated Kodi source tree required')
                audit={}
                names={'xbmc/application/Application.cpp','xbmc/application/ApplicationPlayerCallback.cpp','xbmc/guilib/GUIWindowManager.cpp','xbmc/guilib/GUIComponent.cpp','xbmc/ServiceManager.cpp'}
                candidates=list((trees[0]/'xbmc/application').glob('*.cpp'))
                if len(candidates)>64:raise RuntimeError('Unbounded native application source inventory')
                for candidate in candidates:
                    if candidate.resolve()!=candidate or candidate.stat().st_size>1024*1024:raise RuntimeError('Unsafe native application source')
                    if any(token in candidate.read_text() for token in ('MsgTarget','UnloadSkin','DeInitialize')):names.add(str(candidate.relative_to(trees[0])))
                for candidate in (trees[0]/'xbmc/interfaces/builtins').glob('*.cpp'):
                    if candidate.resolve()!=candidate or candidate.stat().st_size>1024*1024:raise RuntimeError('Unsafe native builtin source')
                    if 'Minimize' in candidate.read_text():names.add(str(candidate.relative_to(trees[0])))
                native_files=list((trees[0]/'xbmc').rglob('*.cpp'))
                if len(native_files)>12000:raise RuntimeError('Unbounded native source inventory')
                for candidate in native_files:
                    if candidate.resolve()!=candidate or candidate.stat().st_size>2*1024*1024:raise RuntimeError('Unsafe native source audit')
                    contents=candidate.read_text()
                    if any(token in contents for token in ('GetWindowManager().DeInitialize','GetWindowManager().DestroyWindows','->UnloadSkin()','case TMSG_MINIMIZE')):names.add(str(candidate.relative_to(trees[0])))
                for name in sorted(names):
                    path=trees[0]/name
                    if path.is_symlink() or path.resolve()!=path:raise RuntimeError('Redirected Kodi audit source: '+name)
                    if not path.exists():audit[name]={'missing':True};continue
                    if not path.is_file() or path.stat().st_size>1024*1024:raise RuntimeError('Unsafe Kodi audit source: '+name)
                    lines=path.read_text().splitlines();selected=set()
                    for index,line in enumerate(lines):
                        if any(token in line for token in ('GUI_MSG_PLAYBACK_STARTED','AddMsgTarget','AddMessageTarget','m_vecMsgTargets','DispatchThreadMessages','OnPlayBackStarted','ResetCurrentItem','DeInitialize','UnloadSkin','Minimize','TMSG_MINIMIZE','DestroyWindows')):
                            selected.update(range(max(0,index-8),min(len(lines),index+24)))
                    audit[name]=dict(sha256=digest(path),lines=[str(index+1)+': '+lines[index] for index in sorted(selected)])
                atomic_json(self.run/'kodi-native-source-audit.json',audit)
                fixtures=[path for path in Path('/tmp').glob('isolde-tv-acceptance-*') if path.is_dir() and not path.is_symlink() and path.stat().st_uid==os.getuid()]
                if len(fixtures)>512:raise RuntimeError('Unbounded acceptance fixture inventory')
                if fixtures:
                    latest=max(fixtures,key=lambda path:path.stat().st_mtime)
                    shell_log=latest/'shell.log'
                    if shell_log.is_file() and not shell_log.is_symlink() and shell_log.stat().st_size<=4*1024*1024:
                        atomic_json(self.run/'kodi-last-fixture-shell.json',{'fixture':str(latest),'lines':shell_log.read_text(errors='replace').splitlines()[-100:]})
                profile_root=Path.home()/'.local/share/Isolde/native-profiles'
                logs=[]
                if fixtures:
                    profile_file=latest/'config/Isolde/profiles.json'
                    if profile_file.resolve()!=profile_file or profile_file.stat().st_size>65536:raise RuntimeError('Unsafe fixture profile identity')
                    fixture_profile=json.loads(profile_file.read_text())['active']
                    if not re.fullmatch(r'[0-9a-f-]{36}',fixture_profile):raise RuntimeError('Invalid fixture profile identity')
                    candidate=profile_root/fixture_profile/'kodi/home/.kodi/temp/kodi.log'
                    if candidate.exists():logs=[candidate]
                if logs:
                    native_log=max(logs,key=lambda path:path.stat().st_mtime)
                    if native_log.resolve()!=native_log or native_log.stat().st_uid!=os.getuid() or native_log.stat().st_size>16*1024*1024:raise RuntimeError('Unsafe native scan log')
                    lines=native_log.read_text(errors='replace').splitlines()
                    atomic_json(self.run/'kodi-last-scan-diagnostics.json',{'log':str(native_log),'lines':[line for line in lines if any(token in line.lower() for token in ('musicinfoscanner','audiolibrary.scan','tagloader','music scan','scanmusic','error <general>'))][-100:]})
                return
            installed=Path('/opt/isolde-managed-kodi')
            if installed.exists():
                paths=[installed,installed/'bin',installed/'kodi.bin',installed/'bin/kodi',installed/'manifest.json']
                for path in paths:
                    if path.resolve()!=path or path.stat().st_uid!=0:raise RuntimeError('Unsafe managed Kodi package path/owner')
                if any(path.stat().st_mode&0o022 for path in paths):
                    self.stage('kodi-package-mode-repair',['python3',str(source/'tools/build-managed-kodi.py'),'--source',str(source),'--work',str(self.root/'support/managed-kodi'),'--receipt',str(self.run/'kodi-mode-repair.json'),'--repair-package-modes'],480)
            index_lease=self.root/'kodi-indexer-lease.json'
            if shutil.disk_usage(self.root).free<3*1024**3 and (Path.home()/'.cache/tracker3').is_dir():
                listing=self.command('systemctl','--user','list-units','--all','--plain','--no-legend')
                units=[]
                for line in listing.splitlines():
                    fields=line.split()
                    if len(fields)>=4 and fields[2]=='active' and re.fullmatch(r'(?:tracker-[A-Za-z0-9_.@-]+|localsearch-3|tinysparql-[A-Za-z0-9_.@-]+)\.service',fields[0]):units.append(fields[0])
                if units:
                    atomic_json(index_lease,{'units':units,'guest':'isolde-test','reason':'managed Kodi dependency build'})
                    self.stage('pause-guest-indexer',['systemctl','--user','stop',*units],30)
                mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
                reclaimed=remove_guest_index_cache(Path.home()/'.cache/tracker3',Path.home(),mounts)
                atomic_json(self.run/'guest-index-cache-cleanup.json',{'guest':'isolde-test','cache':str(Path.home()/'.cache/tracker3'),'reclaimedBytes':reclaimed,'normalProfilesPreserved':True,'unitsPaused':units})
            self.automatic_test_cleanup()
            self.check()
            # The frozen release is retained separately. A dependency rebuild
            # may discard even the newest unused legacy CMake cache, preserving
            # its authored source and LastTest evidence. This is guest-only.
            legacy=Path.home()/'isolde-runs';removed=[]
            if shutil.disk_usage(self.root).free<4*1024**3 and legacy.is_dir() and not legacy.is_symlink() and legacy.resolve()==legacy:
                mounts=[Path(line.split()[4].replace('\\040',' ')) for line in Path('/proc/self/mountinfo').read_text().splitlines()]
                active=[]
                for process in Path('/proc').iterdir():
                    if process.name.isdigit():
                        for name in ('cwd','exe'):
                            try:active.append((process/name).resolve(strict=True))
                            except (OSError,RuntimeError):pass
                for candidate in sorted(legacy.iterdir()):
                    if shutil.disk_usage(self.root).free>=4*1024**3:break
                    if candidate.is_symlink() or not re.fullmatch(r'studio-implementation-[0-9]+',candidate.name):continue
                    target=candidate/'build';cache=target/'CMakeCache.txt'
                    if not cache.is_file() or cache.is_symlink() or any(path==target or path.is_relative_to(target) for path in active):continue
                    roots=re.findall(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$',cache.read_text(),re.M)
                    if len(roots)!=1:continue
                    authored=Path(roots[0])
                    if authored.resolve()!=authored or authored==target or authored.is_relative_to(target) or not (authored/'CMakeLists.txt').is_file():continue
                    saved=self.run/'legacy-build-evidence'/candidate.name;saved.mkdir(parents=True,exist_ok=True)
                    for relative in ('CMakeCache.txt','Testing/Temporary/LastTest.log','Testing/Temporary/LastTestsFailed.log','ctest.xml'):
                        item=target/relative
                        if item.is_file() and not item.is_symlink() and item.stat().st_size<=16*1024**2:shutil.copy2(item,saved/relative.replace('/','-'))
                    removed.append({'build':str(target),'authoredSourcePreserved':str(authored),'evidence':str(saved)})
                    atomic_json(self.run/'kodi-legacy-build-cleanup.json',{'guest':'isolde-test','targets':removed,'installedReleasePreserved':True})
                    remove_guest_build(target,legacy,mounts)
            self.stage('kodi-managed-build',['python3',str(source/'tools/build-managed-kodi.py'),'--source',str(source),'--work',str(self.root/'support/managed-kodi'),'--receipt',str(self.run/'kodi-managed-result.json')],7200)
            self.restore_kodi_indexer()
            self.update(service=self.service(),releases=self.links())
            return
        if target=='store-test-data':
            payload=self.tested_payload();release=self.deployed_release(payload,target)
            if self.command('hostname')!='isolde-test':raise RuntimeError('Guest-only fixture cleanup')
            helper=verified_studio_helper(self.root,store_helper_digest,'store')
            with exclusive_lock(Path('/tmp/isolde-home-components-acceptance.lock')):
                self.stage('store-test-data',store_helper_command(self.run/'source',helper,Path(release)/'bin/isolde',['--discard-completed-data']),300)
            return
        if target=="kodi-demo-art":
            payload=self.tested_payload();self.deployed_release(payload,target)
            if self.command('hostname')!='isolde-test' or not isinstance(demo_art_helper_digest,str) or not re.fullmatch(r'[a-f0-9]{64}',demo_art_helper_digest):raise RuntimeError('Demo artwork setup requires the tested Isolde VM and a verified helper')
            helper=self.root/'bin'/('kodi-demo-art-'+demo_art_helper_digest+'.py')
            if helper.resolve()!=helper or digest(helper)!=demo_art_helper_digest:raise RuntimeError('Demo artwork helper identity mismatch')
            with exclusive_lock(Path('/tmp/isolde-home-components-acceptance.lock')):
                self.stage('kodi-demo-art',[sys.executable,str(helper),'--source',str(self.run/'source'),'--receipt',str(self.run/'kodi-demo-art-result.json')],900)
            self.update(service=self.service(),releases=self.links())
            return
        if target in ("artifacts","desktop-prompts","storage-capacity"):
            if self.command("hostname")!="isolde-test" or self.root!=Path.home()/"isolde-oracle" or self.root.resolve()!=self.root:
                raise RuntimeError("Artifact maintenance is restricted to the native Isolde-Test oracle")
            if not isinstance(maintenance_tests_digest,str) or not re.fullmatch(r"[0-9a-f]{64}",maintenance_tests_digest):
                raise RuntimeError("Artifact maintenance requires matching runner self-tests")
            driver_digest=digest(Path(__file__))
            bundle=self.root/"bin"/("maintenance-"+driver_digest+"-"+maintenance_tests_digest)
            tests=bundle/"tests/test_vm_oracle.py"
            hashes=bundle/"files.sha256"
            if bundle.resolve()!=bundle or hashes.resolve()!=hashes or digest(hashes)!=maintenance_tests_digest:
                raise RuntimeError("Maintenance self-test bundle checksum mismatch")
            entries=manifest(hashes)
            if set(entries)!={"tools/store-acceptance.py","tools/install-appliance.py","tools/vm-oracle.py","tests/test_vm_oracle.py","tools/home-reference-acceptance.py","tools/home-components-acceptance.py","tools/reference_collections.py","tests/tst_home_navigation.cpp","tools/guide_features.py","tools/guide_lifecycle.py","tools/guide-soak-acceptance.py","qml/OnScreenKeyboard.qml","CMakeLists.txt","config/studio-defaults.json","tests/fixtures/shell-theme-routes.json"} or entries["tools/vm-oracle.py"]!=driver_digest:
                raise RuntimeError("Unexpected maintenance self-test dependencies")
            if any((bundle/name).resolve()!=bundle/name for name in entries):
                raise RuntimeError("Redirected maintenance self-test dependency")
            verify_tree(bundle,entries)
            self.stage("guest-maintenance-self-tests",[sys.executable,str(tests)],90)
            if target=='storage-capacity':
                self.storage_retention(apply=apply_storage_retention)
                if apply_storage_retention:
                    self.exclude_test_trees_from_indexer()
                    self.restore_kodi_indexer()
                return
            if target=='desktop-prompts':
                self.prepare_visual_desktop()
                return
            remove_generated_payload_cache(self.run)
            self.maintain_artifacts()
            return
        payload=self.tested_payload()
        if target=="guide":
            if self.command("hostname")!="isolde-test":raise RuntimeError("PVR setup is restricted to Isolde-Test")
            self.prepare_guest_space(3)
            self.stage("guide-pvr-install",["python3",str(self.run/"source/tools/provision-guide.py"),"--receipt",str(self.run/"guide-pvr-install.json")],300)
            return
        if target=="games":
            if self.command("hostname")!="isolde-test":raise RuntimeError("Games setup is restricted to Isolde-Test")
            self.prepare_guest_space()
            self.stage("games-runtime-install",["python3",str(self.run/"source/tools/provision-games.py"),"--install","--fixture","--receipt",str(self.run/"games-runtime-install.json")],2400)
            return
        self.deployed_release(payload, target)
        if self.command("hostname")!="isolde-test":
            raise RuntimeError("Native configuration is restricted to Isolde-Test")
        if target=='android-store-support':
            self.stage('android-compositor-install',['sudo','-n','/usr/bin/apt-get','install','-y','--no-install-recommends','weston'],180)
            release=self.deployed_release(payload,target);support=Path(release)/'share/isolde'
            package_root=self.run/'android-support-package';package_root.mkdir(mode=0o700)
            bootstrap="import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('setup',Path(sys.argv[1])/'android-setup.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.build_support(Path(sys.argv[1]),Path(sys.argv[2]))"
            self.stage('android-support-package',['python3','-B','-c',bootstrap,str(support),str(package_root)],90)
            self.stage('android-support-install',['sudo','-n','/usr/bin/dpkg','-i',str(package_root/'isolde-android-support.deb')],180)
            check="import importlib.util,json,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('setup',Path(sys.argv[1])/'android-setup.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.trusted_support();print(Path('/var/lib/isolde/android-store-capabilities.json').read_text())"
            self.stage('android-support-verify',['python3','-B','-c',check,str(support)],30)
            atomic_json(self.run/'android-support-result.json',{'guest':'isolde-test','passed':True,'support':'/usr/lib/isolde-android-setup','imagesReadOnly':True,'runtimeAndProfilesPreserved':True})
            return
        if target=='audio-test-recover':
            if self.command('hostname')!='isolde-test':raise RuntimeError('Audio test recovery is restricted to Isolde-Test')
            before=self.service()
            try:
                self.stage('audio-recover-stop-shell',['systemctl','--user','stop','isolde.service'],30)
                self.stage('audio-recover-guest-services',['systemctl','--user','restart','pipewire.service','pipewire-pulse.service','wireplumber.service'],45)
                states=self.command('systemctl','--user','show','pipewire.service','pipewire-pulse.service','wireplumber.service','--property=Id,ActiveState,SubState')
                if states.count('ActiveState=active')!=3:raise RuntimeError('Guest audio services did not recover')
                atomic_json(self.run/'guest-audio-recovery.json',{'guest':'isolde-test','serviceBefore':before,'audioServices':states,'settingsChanged':False,'hostChanged':False})
            finally:
                if before.get('ActiveState')=='active':self.stage('audio-recover-restore-shell',['systemctl','--user','start','isolde.service'],30)
            return
        if target=="browser-test-tools":
            if self.command('hostname')!='isolde-test':raise RuntimeError('Browser test setup is restricted to Isolde-Test')
            self.stage('browser-test-tools-install',['sudo','-n','/usr/bin/apt-get','install','-y','--no-install-recommends','xvfb'],180)
            if not shutil.which('Xvfb'):raise RuntimeError('Guest Xvfb installation did not supply its executable')
            atomic_json(self.run/'browser-test-tools.json',{'guest':'isolde-test','package':'xvfb','tool':shutil.which('Xvfb'),'hostChanged':False})
            return
        if target=="audio-test-tools":
            # Test-only guest package, through its pre-existing delegated apt
            # permission. No repository, host or application dependency change.
            self.stage("audio-test-tools-install",["sudo","-n","/usr/bin/apt-get","install","-y","--no-install-recommends","pulseaudio-utils"],180)
            for tool in ("pactl","parec"):
                if not shutil.which(tool):raise RuntimeError("Missing installed audio test utility: "+tool)
            atomic_json(self.run/"audio-test-tools.json",{"guest":"isolde-test","package":"pulseaudio-utils","tools":{tool:shutil.which(tool) for tool in ("pactl","parec")}})
            return
        if target=="android":self.prepare_guest_space(8)
        script=self.run/"source/tools/provision-default-apps.py"
        command=["/usr/bin/python3",str(script),"isolation","--source",str(self.run/"source")]
        # Older tested installers inherited the administrator's group-writable
        # umask. Tighten only the exact approved directory after installation.
        command=["/usr/bin/python3","-c","import os,runpy,sys; sys.argv="+repr(command[1:])+"; runpy.run_path(sys.argv[0],run_name='__main__'); os.chmod('/usr/lib/isolde-native',0o755)"]
        if target=="surfshark":command=["/usr/bin/python3",str(script),"surfshark","--source",str(self.run/"source")]
        if target=="android":command=["/usr/bin/python3",str(self.run/"source/tools/provision-android.py"),"--source",str(self.run/"source"),"--software-rendering"]
        if target=="android-repair":command=["/usr/bin/python3",str(self.run/"source/tools/provision-android.py"),"--source",str(self.run/"source"),"--repair-runtime"]
        launcher=self.run/"native-admin-terminal.py"
        title="Isolde Android setup" if target.startswith("android") else "Isolde Surfshark setup" if target=="surfshark" else "Isolde native setup"
        launcher.write_text("import subprocess\nprint("+repr(title+" — enter the VM administrator password only here.")+", flush=True)\nr = subprocess.run("+repr(["pkexec"]+command)+")\nprint('Setup finished with exit code', r.returncode, flush=True)\n"+("if r.returncode: input('Press Enter to close this window. ')\n" if target in {"surfshark","android"} else "input('Press Enter to close this window. ')\n")+"raise SystemExit(r.returncode)\n")
        if target.startswith("android"):
            # Capture installer output, never the administrator authentication
            # dialog. Errors close automatically and produce a structured exit.
            (self.run/"android-admin-exit.json").unlink(missing_ok=True)
            launcher.write_text(android_admin_launcher(command,self.run,title))
        environment=dict(os.environ)
        for line in self.command("systemctl","--user","show-environment").splitlines():
            key,_,value=line.partition("=")
            if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}: environment[key]=value
        before,links,layouts=self.service(),self.links(),self.layout_hashes()
        atomic_json(self.root/"service-lease.json",{"service":before,"releases":links,"runId":self.state["runId"],"pid":os.getpid()})
        try:
            self.stage("native-configure-stop",["systemctl","--user","stop","isolde.service"],30)
            print("VM_AUTH_REQUIRED: authenticate in the "+title+" terminal inside Isolde-Test; never send a password here.",flush=True)
            self.stage("native-configure-policy",["ptyxis","--standalone","--title",title,"--","/usr/bin/python3",str(launcher)],3600 if target.startswith("android") else 900 if target=="surfshark" else 180,env=environment)
            if target.startswith("android"):
                outcome=self.run/"android-admin-exit.json"
                if not outcome.is_file() or json.loads(outcome.read_text()).get("exit")!=0:raise RuntimeError("Android administrator setup failed; see android-admin.log")
                receipt=Path("/var/lib/isolde/android-install.json")
                if receipt.is_symlink() or not receipt.is_file() or receipt.stat().st_size>65536 or receipt.stat().st_uid!=0:raise RuntimeError("Android administrator setup did not produce a valid receipt")
                result=json.loads(receipt.read_text())
                expected=digest(self.run/"source/config/android-runtime.json")
                if result.get("configurationSha256")!=expected or result.get("apkStaged") is not True or result.get("profileAppInstalled") is not False or result.get("launchVerified") is not False:raise RuntimeError("Android setup receipt does not match this tested configuration")
                atomic_json(self.run/"android-install-result.json",result)
            elif target=="surfshark":
                receipt=Path("/var/lib/isolde/surfshark-install.json")
                if not receipt.is_file():raise RuntimeError("Surfshark administrator setup did not complete")
                result=json.loads(receipt.read_text())
                if not result.get("installed"):raise RuntimeError("Surfshark installation was not confirmed")
                atomic_json(self.run/"surfshark-install-result.json",result)
            elif not Path("/usr/lib/isolde-native/bwrap").is_file() or not Path("/etc/apparmor.d/isolde-native").is_file():
                raise RuntimeError("Administrator setup did not complete; no native sandbox exception installed")
            if target=="native":self.stage("native-configure-isolation-test",["ctest","--test-dir",str(self.root/"work/build"),"-R","^native_profile_isolation$","--output-on-failure"],90)
        finally:
            self.update(service=self.restore(before),normalLayoutsBefore=layouts,normalLayoutsAfter=self.layout_hashes())
            (self.root/"service-lease.json").unlink(missing_ok=True)
            if self.links()!=links or self.layout_hashes()!=layouts: raise RuntimeError("Native configuration changed a release or saved layout")
        if target=="surfshark":
            # The VPN control app runs in the guest desktop, not a private media
            # app sandbox. Its system daemon must manage this guest's network.
            self.stage("surfshark-open",["/usr/bin/python3","-c","import subprocess; subprocess.Popen(['gtk-launch','surfshark'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)"],15,env=environment)

    def diagnose(self, app_id="freetube"):
        if app_id not in {"freetube", "cbc-gem", "steam"}: raise ValueError("Unsupported native diagnostic")
        self.tested_payload()
        profile_id = "8bdf6077-4613-4b52-9b20-d1ec7931dfcf"
        profiles = json.loads((Path.home()/".config/Isolde/profiles.json").read_text())
        if profiles.get("active") != profile_id or not any(p.get("id")==profile_id and p.get("name")=="Studio Demo" for p in profiles.get("profiles", [])):
            raise RuntimeError("Select the explicitly authorized Studio Demo profile before native diagnostics")
        work = self.root/"work"
        if json.loads((work/"manifest.json").read_text()) != manifest(self.run/"files.sha256"):
            raise RuntimeError("Native diagnostic helper does not match this tested source")
        executable = work/"build/isolde_native_smoke"
        if not executable.is_file(): raise RuntimeError("Tested native diagnostic helper is unavailable")
        environment=dict(os.environ)
        for line in self.command("systemctl","--user","show-environment").splitlines():
            key,_,value=line.partition("=")
            if key in {"DISPLAY","XAUTHORITY","XDG_RUNTIME_DIR","DBUS_SESSION_BUS_ADDRESS"}: environment[key]=value
        if not environment.get("DISPLAY"): raise RuntimeError("Native diagnostics require the logged-in graphical session")
        before, links, layouts = self.service(), self.links(), self.layout_hashes()
        atomic_json(self.root/"service-lease.json", {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()})
        try:
            self.stage("native-diagnostic-stop", ["systemctl", "--user", "stop", "isolde.service"], 30)
            # Existing production-bridge helper, same explicitly authorized
            # Studio Demo profile. No shell, shared home, login or policy change.
            seconds=30 if app_id=="steam" else 5
            self.stage("native-"+app_id+"-diagnostic", [str(executable), app_id, profile_id, str(seconds)], seconds+15, env=environment)
        finally:
            self.update(service=self.restore(before), normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes(), diagnosticOnly=True)
            (self.root/"service-lease.json").unlink(missing_ok=True)
            if self.links()!=links or self.layout_hashes()!=layouts:
                raise RuntimeError("Native diagnostic changed a release or saved layout")

    def visual(self, case, installed=False, reopen_helper_digest=None, feature_helper_digest=None, studio_helper_digest=None, history_helper_digest=None, reference_helper_digest=None, reference_collections_digest=None, diagnostic=False, store_helper_digest=None):
        self.prepare_visual_desktop()
        if diagnostic and installed:raise RuntimeError('Diagnostic payloads cannot be deployed or tested as installed')
        if store_helper_digest and case not in ('store','store-web','store-offline','store-updates','store-discovery'):raise RuntimeError('Store helper is Store-only')
        if reference_collections_digest and (not reference_helper_digest or case not in ('lounge','pastel')):raise RuntimeError('Reference collections helper is Lounge/Pastel-only')
        if reference_helper_digest and case not in ('lounge','pastel'):raise RuntimeError('Reference helper is Lounge/Pastel-only')
        if history_helper_digest and case not in ('local-search','history','pages','playlists','movieart','games','emulators'):raise RuntimeError('Unsupported library helper case')
        if feature_helper_digest and case!='guide-features':raise RuntimeError('Feature helper is guide-features-only')
        if studio_helper_digest and case not in ('studio','studio-designs','studio-search','themes','themecustom','themefaults','themecontrols','sources','desktop','collections','catalog'):raise RuntimeError('Studio helper requires a Studio/desktop case')
        if case in ("repairs", "apps", "clients", "vpn", "shell", "androidapps") and not installed:
            raise RuntimeError("The explicit Studio Demo repair journey requires the verified installed release")
        payload = self.tested_payload(diagnostic=diagnostic) if diagnostic else self.tested_payload()
        if installed:
            deployed = json.loads((self.run / "deploy-result.json").read_text())
            executable = Path(deployed["release"]) / "bin/isolde"
            if self.links()["current"] != deployed["release"] or digest(executable) != digest(payload / "bin/isolde"):
                raise RuntimeError("Installed release does not match this tested run")
        else:
            executable = payload / "bin/isolde"
        print("EXECUTABLE " + str(executable), flush=True)
        source = self.run / "source"
        script, flags = CASES[case]
        command = ["python3", str(source / "tools" / script), str(executable)] + flags
        if store_helper_digest:
            helper=verified_studio_helper(self.root,store_helper_digest,'store')
            command=store_helper_command(source,helper,executable,flags)
            self.update(acceptanceHelperSha256=store_helper_digest)
        if reference_helper_digest:
            helper=verified_studio_helper(self.root,reference_helper_digest,'reference')
            collections_helper=verified_studio_helper(self.root,reference_collections_digest,'reference-collections') if reference_collections_digest else None
            command=reference_helper_command(source,helper,executable,flags,collections_helper)
            if reference_collections_digest:self.update(referenceCollectionsHelperSha256=reference_collections_digest)
            self.update(acceptanceHelperSha256=reference_helper_digest)
        if case in ('local-search','history','pages','playlists','movieart','games','emulators') and history_helper_digest:
            helper=verified_studio_helper(self.root,history_helper_digest,'history')
            command=history_helper_command(source,helper,executable,local_search=case=='local-search',pages=case=='pages',playlists=case=='playlists',movie_art=case=='movieart',game_case=case if case in ('games','emulators') else None)
            self.update(acceptanceHelperSha256=history_helper_digest)
        if case in ('studio','studio-designs','studio-search','themes','themecustom','themefaults','themecontrols','sources','desktop','collections','catalog') and studio_helper_digest:
            helper=verified_studio_helper(self.root,studio_helper_digest)
            gallery_path,gallery_digest=verified_gallery_evidence(self.run,json.loads((self.run/'test-result.json').read_text())) if case=='collections' else (None,None)
            command=studio_helper_command(source,helper,executable,desktop=case=='desktop',collections=case=='collections',catalog=case=='catalog',designs=case=='studio-designs',search=case=='studio-search',themes=case=='themes',themefaults=case=='themefaults',themecustom=case=='themecustom',themecontrols=case=='themecontrols',sources=case=='sources',gallery_evidence=gallery_path,gallery_digest=gallery_digest)
            self.update(acceptanceHelperSha256=studio_helper_digest)
        if case=='guide-features' and feature_helper_digest:
            helper=verified_guide_helper(self.root,feature_helper_digest,'features')
            command=feature_helper_command(source,helper,executable,flags)
            self.update(acceptanceHelperSha256=feature_helper_digest)
        if case=="guide-reopen":
            helper=verified_reopen_helper(self.root,reopen_helper_digest)
            arguments=[str(helper),str(executable),'--cold-reopen','--deny-network','--receipt',str(self.run/'guide-soak-receipt.json')]
            command=['python3','-c','import sys,runpy;sys.path.insert(0,sys.argv.pop(1));path=sys.argv[1];sys.argv=sys.argv[1:];runpy.run_path(path,run_name="__main__")',str(source/'tools')]+arguments
            self.update(acceptanceHelperSha256=reopen_helper_digest)
        if case=="guide-soak":command += ['--receipt',str(self.run/'guide-soak-receipt.json')]
        if case in ("lounge", "pastel", "repairs", "apps", "clients"):
            fixture = "home-reference" if case == "lounge" else "home-pastel-reference"
            command += ["--case", str(source / "tests/visual" / fixture)]
        before, links = self.service(), self.links()
        layouts = self.layout_hashes()
        lease = {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()}
        atomic_json(self.root / "service-lease.json", lease)
        evidence = set()

        def line_output(line):
            print(line, flush=True)
            match = re.search(r"/tmp/isolde-(?:components-acceptance|reference-config|tv-acceptance)-[A-Za-z0-9_-]+", line)
            if match:
                evidence.add(Path(match.group()))
        try:
            # The existing harness lock is still authoritative for the graphical session.
            self.stage("visual-" + case, command, 1740 if case in {"themes","guide-lifecycle","guide-features"} else 900 if case in ("studio-designs","pages","history","guide","guide-surfaces","movieart","playlists","android","androidapps","store","store-updates","emulators") else 480, on_line=line_output)
            results = [json.loads((p / "result.json").read_text()) for p in evidence if (p / "result.json").is_file()]
            if not results or not all(visual_result_passed(case, r) for r in results):
                raise RuntimeError("Harness exited without a passing result.json")
            if case=="guide-lifecycle" or (case in {'guide-features','guide-surfaces'} and any((p/'guide-soak-ready.json').is_file() for p in evidence)):
                write_soak_receipt(self.run,evidence,executable)
            self.update(visualCase=case, visualPassed=True, visualReview="pending",
                        visualAcceptance="diagnostic_only" if diagnostic else "pending", semanticAcceptance="not_verified", executable=str(executable))
        finally:
            # This guard also runs on timeouts/SIGHUP when a harness cannot reach its own finally.
            if case in ('store','store-updates') and store_helper_digest:
                for directory in evidence:
                    if (directory/'authorization.json').is_file():
                        cleanup=store_helper_command(source,helper,executable,['--revoke-authorization',str(directory)])
                        self.stage('store-authorization-cleanup',cleanup,90)
            restored = self.restore(before)
            self.update(service=restored, normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes())
            if self.links() != links:
                raise RuntimeError("Release links changed during graphical testing")
            (self.root / "service-lease.json").unlink(missing_ok=True)
            collect_evidence(self.run, case, evidence)
            if self.layout_hashes() != layouts:
                raise RuntimeError("Normal profile layout changed during an isolated visual test")

    def layout_hashes(self):
        config = Path.home() / ".config/Isolde"
        return {str(p.relative_to(config)): digest(p) for p in config.rglob("home-layout.json")
                if p.is_file() and not p.is_symlink()}

    def deploy(self):
        payload = self.tested_payload()
        if (self.root/'storage-policy.json').is_file():self.storage_retention(apply=True)
        source = self.run / "source"
        spec = importlib.util.spec_from_file_location("oracle_installer", source / "tools/install-appliance.py")
        installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(installer)
        if os.geteuid() == 0 or not installer.ubuntu_supported():
            raise RuntimeError("Deploy requires the regular user on supported Ubuntu")
        prefix = Path.home() / ".local/share/isolde-appliance"
        prefix.mkdir(parents=True, exist_ok=True)
        with exclusive_lock(prefix / ".install.lock"):
            before = self.links()
            was_active = self.service().get("ActiveState") == "active"
            activation_started = False
            try:
                with release_link_transaction(installer, prefix):
                    activation_started = True
                    release = installer.install_payload(payload, prefix)
                    self.stage("android-app-registration", ["python3", str(source/"tools/provision-android.py"), "--source", str(source), "--activate-if-configured"], 30)
                    self.stage("restart", ["systemctl", "--user", "restart", "isolde.service"], 30)
                    deadline = time.monotonic() + 30
                    while time.monotonic() < deadline:
                        service = self.service()
                        pid = service.get("MainPID", "0")
                        if service["ActiveState"] == "active" and Path(f"/proc/{pid}/exe").resolve() == release / "bin/isolde":
                            result = {"release": str(release), "before": before, "after": self.links(), "service": service}
                            atomic_json(self.run / "deploy-result.json", result)
                            self.update(**result)
                            return
                        time.sleep(0.5)
                    raise RuntimeError("Selected release failed to start; inspect restart.log and journal.")
            except BaseException as error:
                failure = {"error": str(error), "before": before, "after": self.links(), "rollbackVerified": self.links() == before}
                atomic_json(self.run / "deploy-failure.json", failure)
                if not failure["rollbackVerified"]:
                    raise RuntimeError("Deploy failed and release links could not be restored; inspect deploy-failure.json") from error
                if activation_started:
                    self.stage("restore-deploy-service", ["systemctl", "--user", "restart" if was_active else "stop", "isolde.service"], 30)
                failure["service"] = self.service()
                atomic_json(self.run / "deploy-failure.json", failure)
                raise

    def prepare_guest_space(self, target_gib=5, prune_sources=False, prune_payloads=False):
        if self.command("hostname") != "isolde-test":
            raise RuntimeError("Guest provisioning is restricted to Isolde-Test")
        if self.root != Path.home()/"isolde-oracle" or self.root.resolve() != self.root:
            raise RuntimeError("Guest maintenance requires the native oracle directory")
        # Only obsolete generated payloads; source, logs, screenshots, releases,
        # native build cache and all profile/account trees are retained.
        runs = self.root / "runs"
        candidates = sorted((p for p in runs.iterdir() if p.is_dir() and not p.is_symlink()), key=lambda p:p.stat().st_mtime, reverse=True)
        protected = {self.run, *candidates[:3], *getattr(self, 'maintenance_pins', ())}
        if prune_payloads:
            payloads=sorted((p for p in candidates if (p/'payload').is_dir()),key=lambda p:(p/'payload').stat().st_mtime,reverse=True)
            protected.update(payloads[:3])
        # Focused candidates depend on an original full-suite proof. Retain its
        # payload while any existing reuse receipt still pins that run.
        for candidate in candidates:
            record = candidate / "test-result.json"
            if record.is_file() and not record.is_symlink():
                base_id = json.loads(record.read_text()).get("coverageReuse", {}).get("baseRunId", "")
                if re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", base_id):
                    protected.add(runs / base_id)
        # A stopped-profile acceptance receipt still needs its exact executable.
        # Never discard that payload while reclaiming older generated runs.
        protected.update(p for p in candidates if (p/'guide-soak-receipt.json').exists() or (p/'guide-soak-receipt.json').is_symlink())
        if prune_sources and not prune_payloads:
            # Explicit source-copy maintenance retains every staged acceptance
            # payload, including the last passing candidate used for captures.
            protected.update(p for p in candidates if (p/"payload").exists() or (p/"payload").is_symlink())
        mounts = [Path(line.split()[4].replace("\\040", " ")) for line in Path("/proc/self/mountinfo").read_text().splitlines()]
        cleanup_path = self.run / "guest-payload-cleanup.json"
        removed = json.loads(cleanup_path.read_text()).get("targets", []) if cleanup_path.exists() else []
        for candidate in reversed(candidates):
            if shutil.disk_usage(self.root).free >= target_gib * 1024**3: break
            target = candidate / "payload"
            if candidate in protected or not target.is_dir() or target.is_symlink(): continue
            if target.resolve() != target or not target.resolve().is_relative_to(runs.resolve()): raise RuntimeError("Unsafe guest artifact path")
            if any(m == target or m.is_relative_to(target) or (m!=Path('/') and target.is_relative_to(m)) for m in mounts): raise RuntimeError("Refusing mounted guest artifact")
            if prune_payloads:
                record=candidate/'test-result.json'
                if not record.is_file() or record.is_symlink():continue
                tested=json.loads(record.read_text())
                if tested.get('passed') is not True:continue
                if tested.get('fullSuite') is not True:
                    proof=tested.get('coverageReuse')
                    if not isinstance(proof,dict) or not proof:continue
                    if proof!=focused_qml_coverage(self.root,candidate,proof.get('baseRunId'),tested.get('suite')):
                        raise RuntimeError('Obsolete focused payload coverage changed; preserve for inspection')
                remove_generated_payload_cache(candidate)
                if payload_hashes(target)!=tested.get('payload'):raise RuntimeError('Obsolete payload identity mismatch; preserve for inspection')
            removed.append(str(target))
            atomic_json(self.run / "guest-payload-cleanup.json", {"guest": "isolde-test", "targets": removed})
            shutil.rmtree(target)
        # Downloaded distro archives are reproducible cache, not installed
        # packages, profiles or source evidence. Prefer this over deleting
        # immutable source snapshots when obsolete payloads are insufficient.
        if shutil.disk_usage(self.root).free < target_gib * 1024**3:
            cache = Path("/var/cache/apt/archives")
            if cache.is_dir():
                if cache.resolve() != cache or any(m != Path("/") and (cache == m or cache.is_relative_to(m) or m.is_relative_to(cache)) for m in mounts):
                    raise RuntimeError("Refusing redirected or mounted guest package cache")
                before = shutil.disk_usage(self.root).free
                self.stage("guest-apt-cache-clean", ["sudo", "-n", "apt-get", "clean"], 60)
                atomic_json(self.run / "guest-package-cache-cleanup.json", {"guest":"isolde-test", "target":str(cache), "freeBefore":before, "freeAfter":shutil.disk_usage(self.root).free, "installedPackagesRemoved":False})
        legacy = Path.home()/"isolde-runs"
        if legacy.is_dir() and not legacy.is_symlink() and legacy.resolve() == legacy:
            old_runs = sorted((p for p in legacy.iterdir() if p.is_dir() and not p.is_symlink() and re.fullmatch(r"studio-implementation-[0-9]+", p.name)), key=lambda p:int(p.name.rsplit("-", 1)[1]))
            active = []
            for process in Path("/proc").iterdir():
                if process.name.isdigit():
                    for name in ("cwd", "exe"):
                        try: active.append((process/name).resolve(strict=True))
                        except (OSError, RuntimeError): pass
            # Retain the newest legacy build plus every live process tree.
            # The incremental oracle cache is separate and remains untouched.
            for old in old_runs[:-1][:128]:
                if shutil.disk_usage(self.root).free >= target_gib * 1024**3: break
                target = old/"build"
                if not target.is_dir() or any(p == target or p.is_relative_to(target) for p in active): continue
                # Record the checked absolute target before deleting generated
                # artifacts. The helper rejects links, mounts and unknown trees.
                removed.append(str(target))
                atomic_json(cleanup_path, {"guest": "isolde-test", "targets": removed})
                remove_guest_build(target, legacy, mounts)
        if prune_sources:
            receipt = self.run / "guest-source-cleanup.json"
            records = []
            for candidate in reversed(candidates[:512]):
                if shutil.disk_usage(self.root).free >= target_gib * 1024**3: break
                if candidate in protected or (candidate/"payload").exists() or (candidate/"payload").is_symlink() or not (candidate/"source").is_dir(): continue
                state_path=candidate/"status.json"
                if not state_path.is_file() or state_path.is_symlink(): continue
                state=json.loads(state_path.read_text())
                if state.get("state") not in {"passed","failed","cancelled"}: continue
                recovery=[candidate/name for name in ("source.tar.gz","archive.sha256","files.sha256")]
                restored=restore_source_archive(candidate,self.root/'bin')
                if any(not path.exists() and not path.is_symlink() for path in recovery):
                    records.append({"target":str(candidate/"source"),"state":"preserved-missing-recovery-evidence"})
                    atomic_json(receipt,{"guest":"isolde-test","targets":records})
                    continue
                record={"target":str(candidate/"source"),"archive":str(candidate/"source.tar.gz"),"restoredOriginalArchive":restored,"state":"checking"}
                records.append(record)
                atomic_json(receipt,{"guest":"isolde-test","targets":records})
                remove_guest_source_snapshot(candidate,runs,mounts)
                record["state"]="removed"
                atomic_json(receipt,{"guest":"isolde-test","targets":records,"freeGiB":round(shutil.disk_usage(self.root).free/1024**3,2)})
    def provision(self):
        self.tested_payload()
        deployment = json.loads((self.run / "deploy-result.json").read_text())
        if self.links()["current"] != deployment["release"]:
            raise RuntimeError("Provisioning requires this run's deployed release")
        self.prepare_guest_space()
        self.check()
        if shutil.disk_usage(self.root).free < 4 * 1024**3: raise RuntimeError("Default clients need at least 4 GiB free after bounded artifact cleanup")
        before, links, layouts = self.service(), self.links(), self.layout_hashes()
        atomic_json(self.root / "service-lease.json", {"service": before, "releases": links, "runId": self.state["runId"], "pid": os.getpid()})
        source = self.run / "source"
        script = source / "tools/provision-default-apps.py"
        try:
            self.stage("default-apps-stop", ["systemctl", "--user", "stop", "isolde.service"], 30)
            self.stage("default-apps-system", ["python3", str(script), "system", "--sudo-apt", "--source", str(source)], 1800)
            self.stage("default-apps-activate", ["python3", str(script), "activate", "--source", str(source)], 480)
            receipt = Path.home()/".config/Isolde/default-apps-completed.json"
            atomic_json(self.run / "default-apps-result.json", json.loads(receipt.read_text()))
            if self.layout_hashes() != layouts: raise RuntimeError("Application provisioning changed a saved layout")
        finally:
            self.update(service=self.restore(before), normalLayoutsBefore=layouts, normalLayoutsAfter=self.layout_hashes())
            (self.root / "service-lease.json").unlink(missing_ok=True)


def remove_packaged_psp_build(cache,mounts=(),active=(),prune_source=False):
    build=cache/'build';package=cache/'isolde-ppsspp_1.20.4-1_amd64.deb';receipt=cache/'payload.json'
    source=cache/'source'
    if not build.exists() and not (prune_source and source.is_dir()):return []
    if cache.resolve()!=cache or build.resolve()!=build or receipt.resolve()!=receipt or package.resolve()!=package:raise RuntimeError('Redirected PSP build recovery')
    if any(p==cache or p.is_relative_to(cache) for p in active):return []
    if any(m!=Path('/') and (m==cache or m.is_relative_to(cache) or cache.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted PSP build recovery')
    if not receipt.is_file() or receipt.stat().st_size>65536 or not package.is_file():return []
    value=json.loads(receipt.read_text())
    if value.get('sourceCommit')!='fa50bb1976065c4f8b1b47af227d367fe9771555' or value.get('sha256')!=digest(package):raise RuntimeError('PSP offline recovery payload checksum mismatch')
    removed=[]
    if build.exists():
        metadata=build/'CMakeCache.txt'
        if not metadata.is_file() or metadata.resolve()!=metadata or metadata.stat().st_size>1024**2:raise RuntimeError('Unsafe PSP generated build identity')
        if 'CMAKE_HOME_DIRECTORY:INTERNAL='+str(source) not in metadata.read_text().splitlines():raise RuntimeError('Unknown PSP generated source')
        remove_guest_build(build,cache,mounts);removed.append(str(build))
    if prune_source and source.is_dir() and shutil.disk_usage(cache).free<3*1024**3:
        if source.resolve()!=source:raise RuntimeError('Redirected PSP upstream checkout')
        git=['git','-c','core.fsmonitor=false','-c','core.hooksPath=/dev/null','-C',str(source)]
        environment=dict(os.environ,GIT_OPTIONAL_LOCKS='0')
        def query(args):
            result=subprocess.run(git+args,capture_output=True,text=True,check=True,timeout=90,env=environment)
            if len(result.stdout)>1024**2:raise RuntimeError('Unbounded PSP checkout status')
            return result.stdout.strip()
        if query(['rev-parse','HEAD'])!=value['sourceCommit']:raise RuntimeError('PSP checkout identity changed')
        if query(['status','--porcelain','--untracked-files=all','--ignored=matching','--ignore-submodules=none']):raise RuntimeError('PSP checkout has local or ignored files; preserve it')
        if query(['submodule','foreach','--recursive','--quiet','git -c core.fsmonitor=false -c core.hooksPath=/dev/null status --porcelain --untracked-files=all --ignored=matching']):raise RuntimeError('PSP submodule has local or ignored files; preserve it')
        shutil.rmtree(source);removed.append(str(source))
    return removed


def remove_guest_build(target, boundary, mounts):
    """Generated legacy build only, with no redirected or mounted descendants."""
    if target.name != "build" or target.is_symlink() or target.resolve() != target.absolute() or not target.resolve().is_relative_to(boundary.resolve()):
        raise RuntimeError("Unsafe legacy guest build target")
    if not (target/"CMakeCache.txt").is_file() or not (target/"CMakeFiles").is_dir():
        raise RuntimeError("Legacy target is not a generated CMake build")
    if any(m == target or m.is_relative_to(target) for m in mounts): raise RuntimeError("Refusing mounted legacy guest build")
    shutil.rmtree(target)


def collect_evidence(run, case, directories):
    """Do not copy WebEngine caches, PulseAudio cookies or personal profile trees."""
    with tarfile.open(run / ("evidence-" + case + "-" + str(time.time_ns()) + ".tar.gz"), "w:gz") as archive:
        for directory in sorted(directories):
            if not re.fullmatch(r"/tmp/isolde-(?:components-acceptance|reference-config|tv-acceptance)-[A-Za-z0-9_-]+", str(directory)):
                raise RuntimeError("Unexpected evidence root")
            paths = [directory / name for name in ("result.json", "shell.log", "android.log", "window-discovery.json", "attempt-config.json", "provider-requests.json", "authorization.json", "authorization-enabled.json", "authorization-cleanup.json", "store-update-evidence.json")]
            if case in {"guide","guide-surfaces","guide-lifecycle","guide-features"}:
                paths += [directory / name for name in ("pvr-source.json","guide-surfaces-cache.json","guide-soak-ready.json")]
            if case=="guide-lifecycle":paths += [directory/name for name in ('guide-lifecycle.json','guide-soak-ready.json')]
            if case=="movieart":paths += [directory/name for name in ('source-setup.json','source-recovery-inventory.json')]
            if case=="guide-features":paths += [directory/name for name in ('guide-features.json','guide-soak-ready.json')]
            if case in {"guide-soak","guide-reopen"}:paths += [directory/name for name in ('soak-target.json','network-denial.json','cached-artwork-observation.json','original-home-layout.json')]
            if case in {"studio-designs", "studio-search"}:
                paths += [directory / name for name in ("network-denial.json", "portable-export.json")]
            if case=="themefaults":paths += [directory/(tag+"loading-frame.png") for tag in ("dark","pastel","custom")]
            if case=="themecontrols":
                paths += [directory/"sound-output.json",directory/"sound-output-restored.json"]
                paths += [directory/(tag+"sound-routing.json") for tag in ("dark","pastel","custom")]
                paths += [directory/(tag+kind+suffix) for tag in ("dark","pastel","custom") for kind in ("preview","disabled") for suffix in (".pcm","-recorder.log")]
            paths += list((directory / "config/Isolde").glob("home-layout*.json"))
            paths += list((directory / "config/Isolde/skins").glob("*.json"))
            for path in paths[:128]:
                if path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(directory.resolve()):
                    if path.stat().st_size > 8 * 1024**2:
                        continue
                    archive.add(path, arcname=str(Path(directory.name) / path.relative_to(directory)), recursive=False)


def recover_visual_orphan(run, pid, proc_root=Path("/proc")):
    """Stop one proven private child left by a failed visual-helper startup."""
    if socket.gethostname() != "isolde-test" or pid <= 1:
        raise RuntimeError("Private visual recovery requires Isolde-Test and a child PID")
    proc = proc_root / str(pid)
    if not proc.is_dir() or proc.stat().st_uid != os.getuid():
        raise RuntimeError("Visual child is absent or belongs to another user")
    executable = (proc / "exe").resolve(strict=True)
    expected = (run / "payload/bin/isolde").resolve(strict=True)
    if executable != expected:
        raise RuntimeError("Visual child does not use this run's tested executable")
    diagnostic=not (run/'test-result.json').is_file() and (run/'diagnostic-payload.json').is_file()
    result = json.loads((run / ('diagnostic-payload.json' if diagnostic else 'test-result.json')).read_text())
    if diagnostic:diagnostic_campaign_receipt(result,json.loads((run/'campaign-progress.json').read_text()))
    if (not diagnostic and result.get("passed") is not True) or result.get("payload", {}).get("bin/isolde") != digest(executable):
        raise RuntimeError("Visual child executable lacks a matching tested payload")
    values = (proc / "environ").read_bytes().split(b"\0")
    config = next((value.partition(b"=")[2].decode() for value in values if value.startswith(b"XDG_CONFIG_HOME=")), "")
    if not re.fullmatch(r"/tmp/isolde-components-acceptance-[A-Za-z0-9_-]+/config", config):
        raise RuntimeError("Visual child is not using a disposable acceptance profile")
    directory = Path(config)
    if directory.resolve(strict=True) != directory or directory.stat().st_uid != os.getuid():
        raise RuntimeError("Visual child profile is redirected or foreign")
    fixture = str(directory.parent)
    logs = list(run.glob("visual-*.log"))[:64]
    if not any(path.stat().st_size <= 8*1024**2 and fixture in path.read_text() and "NameError" in path.read_text() for path in logs):
        raise RuntimeError("No matching failed-startup evidence for this visual child")
    started = (proc / "stat").read_text().split()[21]
    # Recheck birth identity immediately before signalling; never use pkill or
    # a window PID alone, and never touch the installed service process.
    if (proc / "stat").read_text().split()[21] != started or (proc / "exe").resolve(strict=True) != executable:
        raise RuntimeError("Visual child identity changed during recovery")
    os.kill(pid, signal.SIGTERM)
    deadline = time.monotonic() + 5
    while proc.is_dir() and time.monotonic() < deadline:
        try:
            fields = (proc / "stat").read_text().split()
            if fields[21] != started or fields[2] == "Z": break
        except FileNotFoundError: break
        time.sleep(.1)
    else:
        if proc.is_dir(): raise RuntimeError("Verified visual child did not stop after SIGTERM")
    return {"pid":pid,"processStartTicks":started,"executable":str(executable),"privateConfig":config,"stopped":True,"signal":"SIGTERM"}


def write_report(run):
    """A flat, allowlisted bundle; never the source tree, payload, caches or credentials."""
    with tarfile.open(run / "report.tar.gz.tmp", "w:gz") as archive:
        for path in sorted(run.iterdir()):
            if path.is_file() and not path.is_symlink() and (path.suffix in (".log", ".json", ".xml")
                    or re.fullmatch(r"evidence-(sources|themecontrols|themecustom|themefaults|themes|studio|studio-designs|studio-search|desktop|collections|catalog|local-search|history|lounge|pastel|pages|guide|guide-surfaces|guide-lifecycle|guide-features|guide-soak|guide-reopen|movieart|repairs|apps|clients|vpn|shell|playlists|android|androidapps|store|store-web|store-offline|store-updates|store-discovery|games|emulators)-[0-9]+\.tar\.gz", path.name)
                    or re.fullmatch(r"studio-test-[a-z0-9-]+\.png", path.name)):
                archive.add(path, arcname=path.name, recursive=False)
    (run / "report.tar.gz.tmp").replace(run / "report.tar.gz")


@contextlib.contextmanager
def exclusive_lock(path, wait_seconds=0):
    import fcntl
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a") as handle:
        deadline=time.monotonic()+wait_seconds;last_notice=0
        while True:
            try:
                fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB);break
            except BlockingIOError:
                if time.monotonic()>=deadline:
                    raise RuntimeError("BUSY: another owned run is active. Use Status; do not delete locks or start another build.")
                if time.monotonic()-last_notice>=10:
                    print("WAIT oracle lock; preserving the active owner's work",flush=True);last_notice=time.monotonic()
                time.sleep(min(.25,max(0,deadline-time.monotonic())))
        yield


def interrupted(signum, _frame):
    # selectors treats InterruptedError as a retryable interrupted system call.
    # Cancellation must escape that retry and reach the stage/service finally.
    raise RuntimeError("Oracle interrupted by signal " + str(signum))


def vpn_platform():
    """Public installed-package/NetworkManager metadata, never account storage."""
    def query(arguments):
        result=subprocess.run(arguments,capture_output=True,text=True,timeout=5)
        return result.stdout[:65536] if result.returncode==0 else ""
    version=query(["dpkg-query","-W","-f=${Version}","surfshark"]).strip()
    files=query(["dpkg-query","-L","surfshark"]).splitlines() if version else []
    metadata=[]
    for name in files:
        if not (name.startswith(("/usr/bin/","/usr/share/applications/","/usr/lib/systemd/system/","/lib/systemd/system/")) and (name.endswith((".desktop",".service")) or name.startswith("/usr/bin/"))):continue
        path=Path(name)
        if not path.is_file():continue
        entry={"path":name}
        if path.stat().st_size<=16384:
            content=path.read_bytes()
            if name.endswith((".desktop",".service")):
                entry["declarations"]=[line for line in content.decode("utf-8","replace").splitlines() if line.startswith(("Exec=","ExecStart=","BusName="))][:8]
            elif content.startswith(b"#!"):
                entry["launcher"]=content.decode("utf-8","replace")
        metadata.append(entry)
    connections=[]
    for line in query(["nmcli","-t","-f","UUID,TYPE,DEVICE","connection","show"]).splitlines()[:128]:
        cells=line.split(":")
        if len(cells)==3 and cells[1] in {"wireguard","vpn"}:connections.append({"uuid":cells[0],"type":cells[1],"device":cells[2]})
    bindings=query(["python3","-c","import gi;gi.require_version('NM','1.0');from gi.repository import NM;print(NM.MAJOR_VERSION)"]).strip()
    proton=query(["dpkg-query","-W","-f=${Status} ${Version}","proton-vpn-gnome-desktop"]).strip()
    proton_files=query(["dpkg-query","-L","proton-vpn-cli"]).splitlines()
    permissions=query(["nmcli","-t","-f","PERMISSION,VALUE","general","permissions"]).splitlines()
    reasons=[]
    for line in query(["journalctl","-u","NetworkManager","--since=-10min","-n","200","--no-pager","-o","cat"]).splitlines():
        match=re.search(r"\(ivpn[0-9a-f]{10}\).*reason '([a-z-]+)'",line)
        if match:reasons.append(match[1])
    routing={}
    for family in ("-4","-6"):
        rules=json.loads(query(["ip","-j",family,"rule","show"]) or "[]")
        routing[family]=[{k:r[k] for k in ("priority","table","fwmark","fwmask","not","suppress_prefixlength") if k in r} for r in rules[:64]]
    return {"installedVersion":version,"protonPackage":proton,"protonCliFiles":[p for p in proton_files if p.startswith('/usr/bin/')],"nmBindings":bindings,"packageFiles":metadata[:32],"vpnConnections":connections,"networkManagerPermissions":permissions,"recentIsoldeActivationReasons":reasons[-20:],"routingPolicy":routing}


def cancel_owned(root, run_id):
    path = root / "runs" / run_id / "status.json"
    if not path.is_file():
        return {"cancelled": False, "reason": "unknown run"}
    state = json.loads(path.read_text())
    pid = state.get("pid", 0)
    proc = Path("/proc") / str(pid)
    if state.get("state") != "running" or not proc.is_dir():
        return {"cancelled": False, "reason": "run already ended"}
    command = (proc / "cmdline").read_bytes().decode().split("\0")
    owned_driver = any(p.startswith(str(root / "bin" / "driver-")) and p.endswith(".py") for p in command)
    start_ticks = (proc / "stat").read_text().split()[21]
    if (not owned_driver or "--run-id" not in command or command[command.index("--run-id") + 1] != run_id
            or state.get("processStartTicks") != start_ticks):
        raise RuntimeError("Refusing to signal a process whose ownership/start identity does not match")
    # SIGINT also unwinds older guardians whose SIGTERM handler raised the
    # retryable InterruptedError. Ownership and process-start checks stay above.
    os.kill(pid, signal.SIGINT)
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        state = json.loads(path.read_text())
        if state.get("state") != "running":
            return {"cancelled": True, "state": state}
        if not proc.is_dir():
            state.update(state="failed", error="Cancelled owned runner", updated=time.time())
            atomic_json(path, state)
            return {"cancelled": True, "state": state}
        time.sleep(.2)
    raise RuntimeError("Owned runner has not finished cleanup; preserve evidence and inspect Status")


def remove_managed_kodi_archives(root,mounts=(),active=(),target_free=3*1024**3,installation=Path('/opt/isolde-managed-kodi'),owner_uid=0):
    """Discard only relinkable Ninja archives after the installed Kodi hash passes."""
    work=root/'support/managed-kodi';build=work/'build';ninja=build/'build.ninja'
    if not ninja.exists():return []
    if root.resolve()!=root or work.resolve()!=work or build.resolve()!=build or ninja.resolve()!=ninja:raise RuntimeError('Redirected managed Kodi cache')
    if any(path==build or path.is_relative_to(build) for path in active):return []
    manifest_path=installation/'manifest.json';binary=installation/'kodi.bin'
    for path in (installation,manifest_path,binary):
        info=path.stat()
        if path.resolve()!=path or info.st_uid!=owner_uid or info.st_mode&0o022:raise RuntimeError('Unsafe installed managed Kodi dependency')
    if manifest_path.stat().st_size>8192:raise RuntimeError('Unbounded managed Kodi manifest')
    metadata=json.loads(manifest_path.read_text())
    if metadata.get('feature')!='isolde-sources-v1' or metadata.get('binarySha256')!=digest(binary):raise RuntimeError('Installed managed Kodi checksum mismatch')
    cache=build/'CMakeCache.txt'
    if cache.resolve()!=cache or not cache.is_file() or cache.stat().st_size>1024**2 or ninja.stat().st_size>16*1024**2:raise RuntimeError('Unsafe managed Kodi build graph')
    sources=re.findall(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$',cache.read_text(),re.M)
    if len(sources)!=1 or not Path(sources[0]).is_relative_to(work/'distribution-source') or Path(sources[0]).resolve()!=Path(sources[0]):raise RuntimeError('Unknown managed Kodi source cache')
    candidates=[]
    for relative in re.findall(r'^build ([A-Za-z0-9_./+-]+\.a): CXX_STATIC_LIBRARY_LINKER[^\n]*$',ninja.read_text(),re.M):
        path=build/relative
        if not path.exists() and not path.is_symlink():continue
        if not path.is_relative_to(build) or '..' in path.parts or path.resolve()!=path or not path.is_file() or path.stat().st_uid!=os.getuid():raise RuntimeError('Unsafe managed Kodi archive')
        with path.open('rb') as stream:
            if stream.read(8)!=b'!<arch>\n':raise RuntimeError('Unknown managed Kodi archive')
        candidates.append(path)
    staged=work/'package/opt/isolde-managed-kodi/kodi.bin'
    if staged.exists() or staged.is_symlink():
        if staged.resolve()!=staged or not staged.is_file() or staged.stat().st_uid!=os.getuid() or digest(staged)!=metadata['binarySha256']:raise RuntimeError('Changed managed Kodi package staging binary')
        candidates.append(staged)
    for path in candidates:
        if any(m!=Path('/') and (m==path or m.is_relative_to(path) or path.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted managed Kodi archive')
    removed=[]
    for path in sorted(set(candidates),key=lambda item:item.stat().st_size,reverse=True):
        if shutil.disk_usage(root).free>=target_free:break
        size=path.stat().st_size;path.unlink();removed.append({'path':str(path),'bytes':size})
    return removed


def remove_linked_test_executables(root,mounts=(),active=(),keep=(),target_free=3*1024**3):
    """Ninja can relink generated binaries; retain objects, staged and installed files."""
    build=root/'work/build';targets=build/'CMakeFiles/TargetDirectories.txt'
    if not targets.exists():return []
    if root.resolve()!=root or build.resolve()!=build or targets.resolve()!=targets:raise RuntimeError('Redirected linked-test cache')
    if targets.stat().st_size>1024**2:raise RuntimeError('Unbounded build target list')
    declared=set(targets.read_text().splitlines());candidates=[]
    for path in build.iterdir():
        if not (re.fullmatch(r'isolde(?:_[A-Za-z0-9]+)*_tests',path.name) or path.name in {'isolde','isolde-recovery','isolde-native-host'}) or path.name in keep:continue
        if str(build/'CMakeFiles'/(path.name+'.dir')) not in declared:continue
        if path.resolve()!=path or not path.is_file():raise RuntimeError('Unsafe linked-test executable')
        if any(m!=Path('/') and (m==path or m.is_relative_to(path) or path.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted linked-test executable')
        if path in active:continue
        with path.open('rb') as stream:
            if stream.read(4)!=b'\x7fELF':raise RuntimeError('Unknown linked-test executable')
        candidates.append(path)
    archive=build/'libisolde_core.a'
    if archive.exists() or archive.is_symlink():
        if str(build/'CMakeFiles/isolde_core.dir') not in declared or archive.resolve()!=archive or not archive.is_file():raise RuntimeError('Unsafe generated core archive')
        if any(m!=Path('/') and (m==archive or m.is_relative_to(archive) or archive.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted generated core archive')
        with archive.open('rb') as stream:
            if stream.read(8)!=b'!<arch>\n':raise RuntimeError('Unknown generated core archive')
        if archive not in active:candidates.append(archive)
    removed=[]
    for path in sorted(candidates,key=lambda p:p.stat().st_mtime):
        if shutil.disk_usage(root).free>=target_free:break
        path.unlink();removed.append(str(path))
    return removed


def deduplicate_source_snapshots(root,current,mounts=(),active=(),target_free=3*1024**3):
    """Share identical immutable source bytes without removing any candidate path."""
    runs=root/'runs'
    if root.resolve()!=root or runs.resolve()!=runs:raise RuntimeError('Unsafe source dedup root')
    identical={};records=[]
    for run in sorted(runs.iterdir(),key=lambda p:p.stat().st_mtime,reverse=True)[:1024]:
        if shutil.disk_usage(root).free>=target_free:break
        source=run/'source';state=run/'status.json'
        if run==current or not source.is_dir() or not state.is_file():continue
        if run.resolve()!=run or source.resolve()!=source or state.resolve()!=state:raise RuntimeError('Redirected source dedup candidate')
        if json.loads(state.read_text()).get('state') not in {'passed','failed','cancelled'}:continue
        if any(p==run or p==source or p.is_relative_to(source) for p in active):continue
        if any(m!=Path('/') and (m==source or m.is_relative_to(source) or source.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted source dedup candidate')
        listing=run/'files.sha256'
        if listing.resolve()!=listing or not listing.is_file():raise RuntimeError('Unsafe source dedup manifest')
        entries=manifest(listing);verify_tree(source,entries)
        for name,checksum in entries.items():
            path=source/name
            if path.resolve()!=path or not path.is_file():raise RuntimeError('Unsafe source dedup member')
            stat=path.stat();key=(checksum,stat.st_size,stat.st_mode,stat.st_uid,stat.st_gid,stat.st_dev)
            original=identical.setdefault(key,path)
            if original==path or original.stat().st_ino==stat.st_ino:continue
            if digest(original)!=checksum or digest(path)!=checksum:raise RuntimeError('Source dedup bytes changed')
            temporary=path.with_name('.'+path.name+'.dedup')
            if temporary.exists() or temporary.is_symlink():raise RuntimeError('Existing source dedup transaction')
            os.link(original,temporary,follow_symlinks=False)
            try:
                if digest(temporary)!=checksum or digest(path)!=checksum:raise RuntimeError('Source changed during dedup')
                os.replace(temporary,path)
            finally:
                temporary.unlink(missing_ok=True)
            records.append({'path':str(path),'linkedTo':str(original),'sha256':checksum,'bytes':stat.st_size})
    return records


def remove_unselected_test_objects(root,mounts=(),active=(),keep=(),target_free=3*1024**3,include_production=False):
    """Regenerate unselected test objects; opt in to declared production objects only as a last resort."""
    build=root/'work/build';meta=build/'CMakeFiles';listing=meta/'TargetDirectories.txt'
    if not listing.exists():return []
    if type(include_production) is not bool:raise RuntimeError('Invalid production cache retention policy')
    if root.resolve()!=root or meta.resolve()!=meta or listing.resolve()!=listing or listing.stat().st_size>1024**2:raise RuntimeError('Unsafe test object cache')
    candidates=[]
    for name in listing.read_text().splitlines():
        directory=Path(name)
        if directory.parent!=meta or not (re.fullmatch(r'isolde(?:_[A-Za-z0-9]+)*_tests\.dir',directory.name) or include_production and directory.name in {'isolde_core.dir','isolde.dir'}) or directory.name[:-4] in keep and not (include_production and directory.name in {'isolde_core.dir','isolde.dir'}):continue
        if not directory.is_dir():continue
        if directory.resolve()!=directory:raise RuntimeError('Redirected test object directory')
        if any(m!=Path('/') and (m==directory or m.is_relative_to(directory) or directory.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted test object directory')
        for index,path in enumerate(directory.rglob('*')):
            if index>=10000 or path.resolve()!=path or path.is_symlink():raise RuntimeError('Unsafe test object tree')
            if path.suffix!='.o' or not path.is_file() or path in active:continue
            with path.open('rb') as stream:
                if stream.read(4)!=b'\x7fELF':raise RuntimeError('Unknown generated test object')
            candidates.append(path)
    removed=[]
    for path in sorted(candidates,key=lambda p:p.stat().st_size,reverse=True):
        if shutil.disk_usage(root).free>=target_free:break
        path.unlink();removed.append(str(path))
    return removed


def remove_runner_test_bundles(root, driver, mounts=(), target_free=3*1024**3):
    """Only obsolete, checksum-addressed self-test copies; retain all drivers/evidence."""
    bins=root/'bin'
    if root.resolve()!=root or bins.resolve()!=bins:raise RuntimeError('Redirected runner cache')
    bundles=sorted((p for p in bins.iterdir() if re.fullmatch(r'maintenance-[a-f0-9]{64}-[a-f0-9]{64}',p.name)),key=lambda p:p.stat().st_mtime,reverse=True)
    removed=[]
    for bundle in bundles[3:]:
        if shutil.disk_usage(root).free>=target_free:break
        if driver.resolve().is_relative_to(bundle) or bundle.name.startswith('maintenance-'+digest(driver)+'-'):continue
        if bundle.resolve()!=bundle or not bundle.is_dir():raise RuntimeError('Unsafe runner test bundle')
        if any(m!=Path('/') and (m==bundle or m.is_relative_to(bundle) or bundle.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted runner test bundle')
        hashes=bundle/'files.sha256'
        # Older runner versions did not write a verification receipt. Preserve
        # those unknown copies rather than inventing evidence for deletion.
        if not hashes.exists() and not hashes.is_symlink():continue
        if hashes.resolve()!=hashes or digest(hashes)!=bundle.name.rsplit('-',1)[1]:raise RuntimeError('Runner test bundle checksum mismatch')
        entries=manifest(hashes);verify_tree(bundle,entries)
        allowed=set(entries)|{'files.sha256'}
        for path in bundle.rglob('*'):
            if path.is_symlink():raise RuntimeError('Linked runner test bundle')
            if path.is_file() and path.relative_to(bundle).as_posix() not in allowed:
                match=re.fullmatch(r'([A-Za-z0-9_-]+)\.cpython-[0-9]{2,3}\.pyc',path.name)
                original=path.parent.parent/(match[1]+'.py') if match and path.parent.name=='__pycache__' else None
                if original is None or original.relative_to(bundle).as_posix() not in entries or path.stat().st_size>1024**2:raise RuntimeError('Unknown runner test bundle file')
        shutil.rmtree(bundle);removed.append(str(bundle))
    return removed


def remove_backed_up_archives(root, backups, mounts=(), preserve_recent=False, target_free=None, protected=(), allow_source_copy=False):
    if root.resolve()!=root or not isinstance(backups,list) or len(backups)>1024:raise RuntimeError('Invalid backed-up archive maintenance')
    runs=root/'runs';removed=[]
    recent=set()
    if preserve_recent:
        # Removing generated children changes a run directory's mtime. Retain
        # actual newest source archives using their unchanged upload timestamps.
        recent=set(sorted((p for p in runs.iterdir() if p.is_dir() and not p.is_symlink() and (p/'source.tar.gz').is_file() and not (p/'source.tar.gz').is_symlink()),key=lambda p:(p/'source.tar.gz').stat().st_mtime,reverse=True)[:3])
    for entry in backups:
        if target_free is not None and shutil.disk_usage(root).free >= target_free:break
        identity=entry.get('runId','');checksum=entry.get('sha256','')
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',identity) or not re.fullmatch(r'[a-f0-9]{64}',checksum):raise RuntimeError('Unsafe backed-up archive identity')
        run=runs/identity;archive=run/'source.tar.gz';record=run/'archive.sha256'
        if run in recent or run in protected:continue
        if not archive.exists():continue
        if run.resolve()!=run or archive.resolve()!=archive or record.resolve()!=record:raise RuntimeError('Redirected archive backup')
        if any(m==archive or m.is_relative_to(archive) or (m!=Path('/') and archive.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted archive backup refused')
        if (run/'guide-soak-receipt.json').exists() or (run/'guide-soak-receipt.json').is_symlink():continue
        retained=any((run/name).exists() or (run/name).is_symlink() for name in ('source','payload'))
        if retained and not allow_source_copy:continue
        if retained:
            source=run/'source';listing=run/'files.sha256'
            if source.resolve()!=source or listing.resolve()!=listing:raise RuntimeError('Redirected retained source')
            if not source.is_dir() or not listing.is_file():continue
            verify_tree(source,manifest(listing))
        status=run/'status.json'
        if status.resolve()!=status or not status.is_file() or json.loads(status.read_text()).get('state') not in {'passed','failed','cancelled'}:continue
        if record.read_text().split()[0]!=checksum or digest(archive)!=checksum:raise RuntimeError('Backed-up archive checksum mismatch')
        archive.unlink();removed.append({'runId':identity,'sha256':checksum})
    return removed

def remove_backed_up_reports(root, backups, mounts=(), protected=(), target_free=3*1024**3, preserve_recent=True):
    """Remove only duplicate transport bundles; keep raw evidence and verified host copies."""
    if root.resolve()!=root or not isinstance(backups,list) or len(backups)>1024:raise RuntimeError('Invalid report backup maintenance')
    runs=root/'runs';removed=[]
    if type(preserve_recent) is not bool:raise RuntimeError('Invalid report retention policy')
    recent=set(sorted((p for p in runs.iterdir() if p.is_dir() and not p.is_symlink() and (p/'report.tar.gz').is_file()),key=lambda p:(p/'report.tar.gz').stat().st_mtime,reverse=True)[:3]) if preserve_recent else set()
    for entry in backups:
        if shutil.disk_usage(root).free>=target_free:break
        identity=entry.get('runId','');checksum=entry.get('sha256','')
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}',identity) or not re.fullmatch(r'[a-f0-9]{64}',checksum):raise RuntimeError('Unsafe report backup identity')
        run=runs/identity;archive=run/'report.tar.gz';status=run/'status.json'
        if run in recent or run in protected or (run/'guide-soak-receipt.json').exists() or not archive.exists():continue
        if run.resolve()!=run or archive.resolve()!=archive or status.resolve()!=status:raise RuntimeError('Redirected report backup')
        if any(m!=Path('/') and (m==archive or m.is_relative_to(archive) or archive.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted report backup refused')
        if not status.is_file() or json.loads(status.read_text()).get('state') not in {'passed','failed','cancelled'}:continue
        if digest(archive)!=checksum:raise RuntimeError('Backed-up report checksum mismatch')
        archive.unlink();removed.append({'runId':identity,'sha256':checksum})
    return removed


def remove_failed_source_copies(root, current, mounts=(), active=(), protected=(), target_free=2*1024**3):
    """Drop only archived copies from ended, untested failures, never their evidence."""
    runs=root/'runs'
    if root.resolve()!=root or runs.resolve()!=runs or current.parent!=runs:raise RuntimeError('Unsafe failed source root')
    removed=[]
    for candidate in sorted(runs.iterdir(),key=lambda p:p.stat().st_mtime)[:1024]:
        if shutil.disk_usage(root).free>=target_free:break
        source=candidate/'source';state=candidate/'status.json'
        if candidate==current or candidate in protected or not source.is_dir():continue
        if any((candidate/name).exists() or (candidate/name).is_symlink() for name in ('payload','test-result.json','guide-soak-receipt.json')):continue
        if any(p==candidate or p==source or p.is_relative_to(source) for p in active):continue
        if state.resolve()!=state or not state.is_file():raise RuntimeError('Unsafe failed source state')
        if json.loads(state.read_text()).get('state') not in {'failed','cancelled'}:continue
        if any(not (candidate/name).is_file() for name in ('source.tar.gz','archive.sha256','files.sha256')):continue
        remove_guest_source_snapshot(candidate,runs,mounts)
        removed.append(str(source))
    return removed


def remove_fixture_caches(root, mounts, active, target_free=3*1024**3, preserve_recent=3):
    if root.resolve()!=root or root.is_symlink():raise RuntimeError("Redirected fixture cache root")
    fixtures=sorted((p for p in root.iterdir() if re.fullmatch(r"isolde-(?:tv-acceptance|components-acceptance|reference-config)-[A-Za-z0-9_-]+",p.name) and p.is_dir() and not p.is_symlink()),key=lambda p:p.stat().st_mtime,reverse=True)
    removed=[]
    if preserve_recent not in (0,3):raise RuntimeError('Unsupported fixture cache retention')
    for fixture in fixtures[preserve_recent:]:
        if shutil.disk_usage(root).free>=target_free:break
        cache=fixture/'cache'
        if not cache.exists():continue
        if cache.resolve()!=cache or not cache.is_dir() or any(p==fixture or p.is_relative_to(fixture) for p in active):continue
        if any(m not in {Path('/'),root} and (m==cache or m.is_relative_to(cache) or cache.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted acceptance cache refused: '+str(cache))
        for index,path in enumerate(cache.rglob('*')):
            if index>=10000 or path.is_symlink() or path.resolve()!=path:raise RuntimeError('Linked or excessive acceptance cache refused')
        shutil.rmtree(cache);removed.append(str(cache))
    return removed

def deduplicate_tested_payloads(root, mounts=(), target_free=3*1024**3):
    """Share identical immutable artifacts, including pinned candidates, without removing them."""
    if root.resolve()!=root:raise RuntimeError('Redirected payload dedup root')
    identical={};records=[]
    for run in sorted((root/'runs').iterdir(),key=lambda p:p.stat().st_mtime,reverse=True)[:1024]:
        payload=run/'payload';receipt=run/'test-result.json'
        if not payload.is_dir() or not receipt.is_file():continue
        if run.resolve()!=run or payload.resolve()!=payload or receipt.resolve()!=receipt:raise RuntimeError('Redirected payload dedup candidate')
        if any(m!=Path('/') and (m==payload or m.is_relative_to(payload) or payload.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted payload dedup candidate')
        tested=json.loads(receipt.read_text())
        if tested.get('passed') is not True:continue
        actual=payload_hashes(payload)
        if actual!=tested.get('payload'):raise RuntimeError('Payload identity mismatch during dedup')
        deployment=run/'deploy-result.json'
        if deployment.exists() or deployment.is_symlink():
            if deployment.resolve()!=deployment or deployment.stat().st_size>262144:raise RuntimeError('Unsafe payload deployment receipt')
            deployed=json.loads(deployment.read_text()).get('release')
            if deployed:
                releases=root.parent/'.local/share/isolde-appliance/releases';release=Path(deployed)
                if releases.resolve()!=releases or release.parent!=releases or not re.fullmatch(r'[a-f0-9]{24}',release.name):raise RuntimeError('Unsafe installed dedup source')
                if release.exists() or release.is_symlink():
                    if release.resolve()!=release or not release.is_dir():raise RuntimeError('Redirected installed dedup source')
                    if any(m!=Path('/') and (m==release or m.is_relative_to(release) or release.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted installed dedup source')
                    remove_generated_payload_cache(run,release)
                    if payload_hashes(release)!=actual:raise RuntimeError('Installed payload identity mismatch during dedup')
                    # Only staged destinations are replaced; the running release
                    # remains untouched and supplies already-verified bytes.
                    for name,checksum in actual.items():
                        original=release/name;stat=original.stat()
                        identical.setdefault((checksum,stat.st_mode,stat.st_uid,stat.st_gid,stat.st_dev),original)
        for name,checksum in actual.items():
            if shutil.disk_usage(root).free>=target_free:return records
            path=payload/name;stat=path.stat();key=(checksum,stat.st_mode,stat.st_uid,stat.st_gid,stat.st_dev)
            original=identical.setdefault(key,path)
            if original==path or original.stat().st_ino==stat.st_ino:continue
            temporary=path.with_name('.'+path.name+'.dedup')
            if temporary.exists() or temporary.is_symlink():raise RuntimeError('Existing payload dedup transaction')
            os.link(original,temporary,follow_symlinks=False)
            try:
                if digest(path)!=checksum or digest(temporary)!=checksum:raise RuntimeError('Payload changed during dedup')
                os.replace(temporary,path)
            finally:
                if temporary.exists():temporary.unlink()
            records.append({'path':str(path),'linkedTo':str(original),'sha256':checksum,'bytes':stat.st_size})
    return records


def deduplicate_fixture_media(root, mounts=(), active=(), target_free=3*1024**3, preserve_recent=3):
    """Hardlink identical old synthetic clips; retain every media path and byte."""
    if not root.exists():return []
    if root.resolve()!=root or root.is_symlink():raise RuntimeError('Redirected synthetic media root')
    fixtures=sorted((p for p in root.iterdir() if re.fullmatch(r'Isolde-tv-test-[a-f0-9-]{36}',p.name) and p.is_dir() and not p.is_symlink()),key=lambda p:p.stat().st_mtime,reverse=True)
    records=[]
    for fixture in fixtures[preserve_recent:1024]:
        if shutil.disk_usage(root).free>=target_free:break
        if any(p==fixture or p.is_relative_to(fixture) for p in active):continue
        directory=fixture/'Isolde Television Fixture'
        if not directory.exists():continue
        if directory.resolve()!=directory or not directory.is_dir():raise RuntimeError('Redirected synthetic clip directory')
        if any(m!=Path('/') and (m==directory or m.is_relative_to(directory) or directory.is_relative_to(m)) for m in mounts):raise RuntimeError('Mounted synthetic media refused')
        identical={}
        for path in sorted(directory.glob('Fixture S*.ogv')):
            if not re.fullmatch(r'Fixture S0[01]E0[12]\.ogv',path.name):continue
            if path.resolve()!=path or not path.is_file():raise RuntimeError('Unsafe synthetic clip')
            stat=path.stat()
            if not 4<=stat.st_size<=64*1024**2:raise RuntimeError('Unbounded synthetic clip')
            with path.open('rb') as stream:
                if stream.read(4)!=b'OggS':raise RuntimeError('Unknown synthetic clip format')
            checksum=digest(path);original=identical.setdefault(checksum,path)
            if original==path or original.stat().st_ino==stat.st_ino:continue
            if original.stat().st_dev!=stat.st_dev or original.read_bytes()!=path.read_bytes():raise RuntimeError('Synthetic clip identity changed')
            temporary=path.with_name('.'+path.name+'.dedup')
            if temporary.exists() or temporary.is_symlink():raise RuntimeError('Existing synthetic dedup transaction')
            os.link(original,temporary,follow_symlinks=False)
            try:
                if digest(path)!=checksum or digest(temporary)!=checksum:raise RuntimeError('Synthetic clip changed during dedup')
                os.replace(temporary,path)
            finally:
                if temporary.exists():temporary.unlink()
            records.append({'path':str(path),'linkedTo':str(original),'sha256':checksum,'bytes':stat.st_size})
    return records


def oracle_storage_inventory(root):
    payloads=[];recovery=[];source_copies=[]
    dependency=[]
    prefix=Path('/opt/isolde-managed-kodi')
    for path in (prefix,prefix/'bin',prefix/'kodi.bin',prefix/'bin/kodi',prefix/'manifest.json'):
        if path.exists():
            info=path.stat();dependency.append(dict(path=str(path),resolved=str(path.resolve()),uid=info.st_uid,gid=info.st_gid,mode=oct(info.st_mode&0o7777),bytes=info.st_size))
    for run in sorted((root/"runs").iterdir(),key=lambda p:p.stat().st_mtime,reverse=True)[:1024]:
        if not run.is_dir() or run.is_symlink():continue
        if (run/'source').is_dir() and len(source_copies)<64:
            statefile=run/'status.json'
            state=json.loads(statefile.read_text()) if statefile.is_file() else {}
            source_copies.append(dict(runId=run.name,state=state.get('state'),archive=(run/'source.tar.gz').is_file(),payload=(run/'payload').exists()))
        if (run/'source').is_dir() and not (run/'source.tar.gz').exists() and len(recovery)<6:
            recovery.append({'runId':run.name,'files':[p.name for p in run.iterdir() if p.is_file()][:24]})
        payload=run/"payload"
        if payload.is_dir() and not payload.is_symlink():payloads.append(payload)
    sizes={}
    if payloads:
        result=subprocess.run(["du","-sk","--",*[str(p) for p in payloads]],capture_output=True,text=True,timeout=20)
        if result.returncode:raise RuntimeError("Cannot inventory staged payloads safely")
        for line in result.stdout.splitlines():
            size,_,path=line.partition("\t");sizes[path]=int(size)*1024
    records=[]
    for payload in payloads:
        statefile=payload.parent/"status.json"
        try:state=json.loads(statefile.read_text()) if statefile.is_file() else {}
        except (OSError,ValueError):state={}
        records.append({"runId":payload.parent.name,"payloadBytes":sizes.get(str(payload),0),"modified":payload.stat().st_mtime,"state":state.get("state","unknown")})
    cache_roots=list((Path.home()/'.local/share/Isolde/native-profiles').glob('*/kodi/home/.cache'))[:128]
    extra=[p for p in [Path.home()/'.cache/isolde-emulators',Path.home()/'.cache/isolde/game-runtime-bundles',Path.home()/'Videos/Isolde-Acceptance',*cache_roots] if p.is_dir() and not p.is_symlink() and p.resolve()==p]
    topology={}
    for label,argv in {'blockDevices':['lsblk','--json','--bytes','--output','NAME,PATH,TYPE,SIZE,FSTYPE,MOUNTPOINTS,PKNAME'],'rootMount':['findmnt','--json','--target','/']}.items():
        observed=subprocess.run(argv,capture_output=True,text=True,timeout=10)
        try:topology[label]=json.loads(observed.stdout) if observed.returncode==0 and len(observed.stdout)<=262144 else {'unavailable':True}
        except ValueError:topology[label]={'unavailable':True}
    capacity=shutil.disk_usage(root);topology['filesystemBytes']={'total':capacity.total,'used':capacity.used,'free':capacity.free}
    usage=subprocess.run(["du","-xk","--max-depth=2","--",str(root),"/tmp",str(Path.home()/'.cache'),str(Path.home()/'.local/share/isolde-appliance'),*[str(p) for p in extra]],capture_output=True,text=True,timeout=30)
    directories=[]
    for line in usage.stdout.splitlines():
        size,sep,path=line.partition("\t")
        if sep and size.isdigit():directories.append({"path":path,"bytes":int(size)*1024})
    return {"topology":topology,"managedKodiPaths":dependency,"freeGiB":round(shutil.disk_usage(root).free/1024**3,2),"stagedPayloads":sorted(records,key=lambda r:r["modified"],reverse=True)[:64],"sourceCopies":source_copies,"emulatorCacheEntries":[{"name":p.name,"children":[c.name for c in list(p.iterdir())[:20]] if p.is_dir() and not p.is_symlink() else []} for p in list((Path.home()/".cache/isolde-emulators").iterdir())[:32]] if (Path.home()/".cache/isolde-emulators").is_dir() else [],"sourceRecoveryCandidates":recovery,"largestTestDirectories":sorted(directories,key=lambda r:r["bytes"],reverse=True)[:32]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply-storage-retention",action="store_true")
    parser.add_argument("action", choices=["check", "test", "visual", "deploy", "provision", "configure", "diagnose", "status", "recover", "cancel"])
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--suite", choices=SUITES, default="all")
    parser.add_argument('--reuse-full-run')
    parser.add_argument('--resume-run-id')
    parser.add_argument("--wait-seconds",type=int,default=0)
    parser.add_argument("--storage",action="store_true")
    parser.add_argument('--guide-diagnostics',action='store_true')
    parser.add_argument("--case", choices=CASES, default="studio")
    parser.add_argument("--installed", action="store_true")
    parser.add_argument('--diagnostic',action='store_true')
    parser.add_argument("--native-app", choices=["freetube", "cbc-gem", "steam"], default="freetube")
    parser.add_argument("--configure-target",choices=["native","surfshark","android","android-repair","android-store-support","games","guide","audio-test-tools","browser-test-tools","audio-test-recover","artifacts","desktop-prompts","storage-capacity","kodi-demo-art","store-test-data","kodi-managed","kodi-managed-audit"],default="native")
    parser.add_argument('--demo-art-helper-digest')
    parser.add_argument("--maintenance-tests-digest")
    parser.add_argument("--reopen-helper-digest")
    parser.add_argument("--feature-helper-digest")
    parser.add_argument('--studio-helper-digest')
    parser.add_argument('--history-helper-digest')
    parser.add_argument('--store-helper-digest')
    parser.add_argument('--reference-helper-digest')
    parser.add_argument('--reference-collections-digest')
    parser.add_argument("--orphan-visual-pid", type=int, default=0)
    args = parser.parse_args()
    if args.diagnostic and (args.action!='visual' or args.installed):parser.error('Diagnostic visuals are staged-only')
    if args.resume_run_id and (args.action!='test' or args.suite!='all' or args.reuse_full_run):parser.error('Campaign resumption requires a full Test without reuse')
    if args.orphan_visual_pid and (args.action != "recover" or not 1 < args.orphan_visual_pid < 2**31): parser.error("Visual orphan PID is Recover-only")
    if args.storage and args.action!="status":parser.error("Storage inventory is Status-only")
    if not 0<=args.wait_seconds<=900 or (args.wait_seconds and args.action not in {"test","configure","visual"}):
        parser.error("Bounded lock waiting (0–900 seconds) is restricted to Test/Configure/Visual")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", args.run_id):
        parser.error("Invalid run ID")
    root = Path.home() / "isolde-oracle"
    root.mkdir(exist_ok=True)
    if args.action == "status":
        path = root / "runs" / args.run_id / "status.json"
        status = json.loads(path.read_text()) if path.is_file() else {"state": "unknown", "runId": args.run_id}
        def cpu_ticks():
            result={}
            for proc in list(Path('/proc').iterdir())[:4096]:
                if not proc.name.isdigit():continue
                try:
                    raw=(proc/'stat').read_text();fields=raw.rsplit(')',1)[1].split()
                    result[proc.name]=(fields[19],int(fields[11])+int(fields[12]),raw.split('(',1)[1].rsplit(')',1)[0])
                except (OSError,ValueError,IndexError):pass
            return result
        first=cpu_ticks();sample_started=time.monotonic();time.sleep(.5);second=cpu_ticks();sample_elapsed=time.monotonic()-sample_started
        cpu=[{'pid':int(pid),'name':value[2],'cpuPercent':round((value[1]-first[pid][1])*100/os.sysconf('SC_CLK_TCK')/sample_elapsed,1)} for pid,value in second.items() if pid in first and first[pid][0]==value[0]]
        memory={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in Path('/proc/meminfo').read_text().splitlines()}
        processes=[]
        for proc in list(Path('/proc').iterdir())[:4096]:
            if not proc.name.isdigit():continue
            try:
                fields={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in (proc/'status').read_text().splitlines() if ':' in line}
                processes.append({'pid':int(proc.name),'name':fields.get('Name',''),'rssKiB':int(fields.get('VmRSS','0 kB').split()[0])})
            except (OSError,ValueError):pass
        status['guestResources']={'memory':{key:memory.get(key) for key in ('MemTotal','MemAvailable','SwapTotal','SwapFree','Dirty','Writeback')},'load':Path('/proc/loadavg').read_text().strip(),'largestProcesses':sorted(processes,key=lambda p:p['rssKiB'],reverse=True)[:12],'cpuSampleSeconds':sample_elapsed,'busiestProcesses':sorted(cpu,key=lambda p:p['cpuPercent'],reverse=True)[:8]}
        if args.storage:status["storage"]=oracle_storage_inventory(root)
        status["activeRuns"]=[]
        if args.guide_diagnostics:status['guideBackend']=guide_backend_diagnostics(root/'runs'/args.run_id)
        for candidate in sorted((root/"runs").glob("*/status.json"),key=lambda p:p.stat().st_mtime,reverse=True)[:64]:
            try:
                active=json.loads(candidate.read_text())
                if active.get("state")=="running" and Path("/proc/"+str(int(active["pid"]))).is_dir():
                    status["activeRuns"].append({key:active.get(key) for key in ("runId","stage","action","updated")})
            except (OSError,ValueError,KeyError):continue
        environment = dict(os.environ)
        manager = subprocess.run(["systemctl", "--user", "show-environment"], capture_output=True, text=True, timeout=10)
        for line in manager.stdout.splitlines():
            key, _, value = line.partition("=")
            if key in {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}: environment[key] = value
        windows = subprocess.run(["wmctrl", "-lpx"], env=environment, capture_output=True, text=True, timeout=10)
        status["windowClasses"] = [line.split(None, 5)[:4] for line in windows.stdout.splitlines()[:64]]
        status["isoldeWindowProcesses"] = []
        for window in status["windowClasses"]:
            if len(window) < 4 or window[3] != "isolde.Isolde" or not window[2].isdigit(): continue
            proc = Path("/proc") / window[2]
            record = {"pid": int(window[2]), "exists": proc.is_dir()}
            if proc.is_dir() and proc.stat().st_uid == os.getuid():
                try:
                    record["executable"] = str((proc / "exe").resolve(strict=True))
                    record["processStartTicks"] = (proc / "stat").read_text().split()[21]
                    values = (proc / "environ").read_bytes().split(b"\0")
                    config = next((value.partition(b"=")[2].decode() for value in values if value.startswith(b"XDG_CONFIG_HOME=")), "")
                    if re.fullmatch(r"/tmp/isolde-components-acceptance-[A-Za-z0-9_-]+/config", config):
                        record["privateVisualConfig"] = config
                except (OSError, IndexError, UnicodeError): pass
            status["isoldeWindowProcesses"].append(record)
        status["vpnPlatform"]=vpn_platform()
        status["androidSetup"]={"runtimeExecutable":shutil.which("waydroid"),
            "runtimeConfigured":Path("/var/lib/waydroid/waydroid.cfg").is_file(),
            "apkStaged":Path("/usr/share/isolde/android/smarttube.apk").is_file(),
            "receiptPresent":Path("/var/lib/isolde/android-install.json").is_file(),
            "freeGiB":round(shutil.disk_usage(Path.home()).free/1024**3,2)}
        container_state=subprocess.run(["systemctl","show","waydroid-container.service","-p","ActiveState","-p","SubState","-p","Result","-p","ExecMainStatus"],capture_output=True,text=True,timeout=10)
        status["androidSetup"]["containerService"]=container_state.stdout[:2048]
        status["androidSetup"]["storeFiles"]=[]
        for name in ('/var/lib/isolde','/usr/lib/isolde-android-setup/android-setup.py','/var/lib/waydroid/rootfs/system/build.prop','/var/lib/waydroid/rootfs/system/system/build.prop','/var/lib/waydroid/rootfs/vendor/build.prop','/var/lib/waydroid/waydroid.prop'):
            path=Path(name);record={'path':name}
            try:
                info=path.stat();record.update(size=info.st_size,uid=info.st_uid,mode=oct(info.st_mode & 0o777),symlink=path.is_symlink())
                if path.suffix=='.prop' and info.st_size<=1024**2:
                    wanted={'ro.build.version.sdk','ro.product.cpu.abilist','ro.system.build.version.sdk','ro.system.product.cpu.abilist'}
                    record['properties']={key:value for line in path.read_text().splitlines() for key,sep,value in [line.partition('=')] if sep and key in wanted}
            except OSError as error:record['error']=type(error).__name__
            status['androidSetup']['storeFiles'].append(record)
        container_log=subprocess.run(["journalctl","-u","waydroid-container.service","--since=-15min","-n","60","--no-pager","-o","cat"],capture_output=True,text=True,timeout=10)
        status["androidSetup"]["containerStartupLog"]=container_log.stdout[-12288:]
        # Inspect only this guardian's descendant tree; never unrelated app
        # arguments, environment, account files or terminal contents.
        status["guardianAlive"]=False
        try:
            proc=Path("/proc")/str(status.get("pid",0))
            status["guardianAlive"]=(status.get("processStartTicks")==proc.joinpath("stat").read_text().split()[21]
                and str(root/"bin"/("driver-"+status.get("driverDigest","")+".py")).encode() in proc.joinpath("cmdline").read_bytes().split(b"\0"))
        except (OSError,IndexError):pass
        pending=[status.get("pid",0)] if status["guardianAlive"] else []; seen=set(); names=[]
        while pending and len(seen)<64:
            pid=pending.pop()
            if not isinstance(pid,int) or pid<2 or pid in seen:continue
            seen.add(pid)
            try:
                process=Path("/proc")/str(pid)
                names.append(process.joinpath("comm").read_text().strip()[:40])
                children=process.joinpath("task",str(pid),"children").read_text()[:4096]
                pending.extend(int(p) for p in children.split() if p.isdigit())
            except (OSError,ValueError):pass
        status["androidSetup"]["ownedProcessNames"]=names
        print(json.dumps(status))
        return
    if args.action == "cancel":
        print(json.dumps(cancel_owned(root, args.run_id)), flush=True)
        return
    oracle = Oracle(root, args.run_id)
    acquired = False
    try:
        if args.wait_seconds:
            oracle.update(action=args.action,stage="waiting-for-lock")
            signal.signal(signal.SIGTERM, interrupted);signal.signal(signal.SIGINT, interrupted);signal.signal(signal.SIGHUP, interrupted)
        with exclusive_lock(root / "oracle.lock",args.wait_seconds):
            acquired = True
            signal.signal(signal.SIGTERM, interrupted)
            signal.signal(signal.SIGINT, interrupted)
            signal.signal(signal.SIGHUP, interrupted)
            oracle.update(action=args.action)
            if args.action == "check":
                oracle.check()
            elif args.action == "test":
                try:oracle.test(args.suite,args.reuse_full_run,args.resume_run_id)
                finally:oracle.restore_test_indexer()
            elif args.action == "visual":
                oracle.visual(args.case, args.installed,args.reopen_helper_digest,args.feature_helper_digest,args.studio_helper_digest,args.history_helper_digest,args.reference_helper_digest,args.reference_collections_digest,args.diagnostic,args.store_helper_digest)
            elif args.action == "deploy":
                oracle.deploy()
            elif args.action == "provision":
                oracle.provision()
            elif args.action == "diagnose":
                oracle.diagnose(args.native_app)
            elif args.action == "configure":
                try:oracle.configure_native(args.configure_target,args.maintenance_tests_digest,args.demo_art_helper_digest,args.store_helper_digest,args.apply_storage_retention)
                finally:
                    if args.configure_target=='kodi-managed':oracle.restore_kodi_indexer()
            elif args.action == "recover":
                oracle.restore_test_indexer()
                oracle.restore_kodi_indexer()
                if args.orphan_visual_pid:
                    layouts = oracle.layout_hashes();service = oracle.service();links = oracle.links()
                    receipt = recover_visual_orphan(oracle.run, args.orphan_visual_pid)
                    atomic_json(oracle.run / "orphan-visual-recovery.json", receipt)
                    if oracle.layout_hashes() != layouts or oracle.service() != service or oracle.links() != links:
                        raise RuntimeError("Normal profile/service/release changed during private-child recovery")
                    oracle.update(orphanVisualRecovery=receipt,service=service)
                lease = root / "service-lease.json"
                if lease.is_file():
                    record = json.loads(lease.read_text())
                    if oracle.links() != record["releases"]:
                        raise RuntimeError("Release links changed; inspect manually before recovery")
                    with exclusive_lock(Path("/tmp/isolde-home-components-acceptance.lock")):
                        oracle.update(service=oracle.restore(record["service"]))
                        lease.unlink()

            oracle.update(state="passed", stage="finished")
    except Exception as error:
        if acquired or args.wait_seconds:
            oracle.update(state="failed", error=str(error))
        print("FAIL " + str(error), flush=True)
        sys.exit(1)
    finally:
        if acquired or args.wait_seconds:
            write_report(oracle.run)
    print("VM_RESULT " + json.dumps(oracle.state), flush=True)


if __name__ == "__main__":
    main()
