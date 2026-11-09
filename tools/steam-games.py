#!/usr/bin/env python3
"""Read this Isolde profile's installed Steam titles; supervise direct launches.

No Web API, credential reads, provider writes, custom launch options or shell.
Steam owns authentication, downloads, compatibility, game files and saves.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import time
import uuid

MAX_GAMES = 5000


class SessionCanceled(Exception):
    pass


def app_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[1-9][0-9]{0,9}", value) or int(value) > 4294967295:
        raise ValueError("Invalid Steam game identity")
    return value


def vdf(text):
    """Bounded Valve text KeyValues subset. Refuse ambiguous/duplicate keys."""
    tokens = re.findall(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|\S+', text)
    tokens = [t for t in tokens if not t.startswith("//")]
    if len(tokens) > 32768:
        raise ValueError("Steam metadata exceeds its token limit")
    position = 0
    def string(token):
        if not token.startswith('"') or not token.endswith('"'):
            raise ValueError("Unsupported Steam metadata token")
        value = token[1:-1]
        return re.sub(r'\\([\\"])', r'\1', value)
    def obj(depth=0, nested=False):
        nonlocal position
        if depth > 8:
            raise ValueError("Steam metadata nesting is too deep")
        result = {}
        while position < len(tokens):
            token = tokens[position]; position += 1
            if token == "}":
                if not nested: raise ValueError("Unexpected Steam metadata brace")
                return result
            key = string(token)
            if key in result or position >= len(tokens):
                raise ValueError("Duplicate or incomplete Steam metadata")
            value = tokens[position]; position += 1
            result[key] = obj(depth+1, True) if value == "{" else string(value)
        if nested: raise ValueError("Unclosed Steam metadata object")
        return result
    return obj()


def private_path(path, home, directory=False, external=()):
    home = Path(home).absolute()
    if home.resolve() != home:
        raise ValueError("Steam profile storage is redirected")
    path = Path(path)
    canonical = path.resolve(strict=True)
    allowed = canonical.is_relative_to(home) and canonical != home
    allowed |= any(canonical.is_relative_to(root) for root in external)
    if not allowed:
        raise ValueError("Steam library is outside this Isolde profile")
    if directory and not canonical.is_dir(): raise ValueError("Steam library folder is unavailable")
    return canonical


def read_vdf(path, home, limit, external=()):
    path = Path(path)
    if path.is_symlink(): raise ValueError("Redirected Steam manifest")
    path = private_path(path, home, external=external)
    # Walk the canonical path with directory descriptors. An ancestor replaced
    # after resolve() cannot turn a metadata read into a cross-profile read.
    if len(path.parts)>128: raise ValueError("Steam metadata path is too deep")
    directory=os.open("/",os.O_RDONLY|os.O_DIRECTORY)
    try:
        for part in path.parts[1:-1]:
            child=os.open(part,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=directory)
            os.close(directory);directory=child
        descriptor=os.open(path.name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=directory)
        with os.fdopen(descriptor,"rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):raise ValueError("Steam metadata is not a regular file")
            data = stream.read(limit+1)
    finally:os.close(directory)
    if len(data) > limit or b"\0" in data: raise ValueError("Invalid Steam metadata size or contents")
    return vdf(data.decode("utf-8-sig"))


def external_roots(values):
    roots=[]
    for value in values:
        if not isinstance(value,str) or len(value)>4096:
            raise ValueError("Invalid external Steam library path length")
        path=Path(value)
        if not path.is_absolute() or str(path)!=str(path.resolve(strict=True)):
            raise ValueError("External Steam library path is redirected or unavailable")
        if not (path.is_relative_to(Path("/media")) or path.is_relative_to(Path("/mnt"))) or path in (Path("/media"),Path("/mnt")):
            raise ValueError("External Steam libraries must be approved mount folders")
        steamapps=path/"steamapps"
        if not steamapps.is_dir() or steamapps.is_symlink() or steamapps.resolve(strict=True)!=steamapps:
            raise ValueError("External Steam library has no steamapps folder")
        if path in roots: raise ValueError("Duplicate external Steam library")
        for existing in roots:
            if path in existing.parents or existing in path.parents:
                raise ValueError("Nested external Steam libraries are not supported")
        roots.append(path)
    if len(roots)>8: raise ValueError("Too many external Steam libraries")
    return roots


def libraries(home, external=()):
    home = Path(home).absolute()
    approved=external_roots(external)
    roots = []
    warnings = []
    for relative in (".local/share/Steam/steamapps", ".steam/steam/steamapps", ".steam/root/steamapps"):
        candidate = home/relative
        if not candidate.exists(): continue
        root = private_path(candidate, home, True)
        if root not in roots: roots.append(root)
    for root in list(roots):
        index = root/"libraryfolders.vdf"
        if not index.exists(): continue
        folders = read_vdf(index, home, 1024*1024, approved).get("libraryfolders", {})
        if not isinstance(folders, dict) or len(folders) > 32: raise ValueError("Too many Steam libraries")
        for number, entry in folders.items():
            if not number.isdigit(): continue
            name = entry.get("path") if isinstance(entry, dict) else entry
            if not isinstance(name, str) or not Path(name).is_absolute(): raise ValueError("Invalid Steam library path")
            library=Path(name)
            candidate = library/"steamapps"
            if not candidate.exists(): continue
            try: candidate = private_path(candidate, home, True, approved)
            except ValueError:
                warnings.append("External Steam libraries are not approved for this Isolde profile."); continue
            if candidate not in roots: roots.append(candidate)
            if len(roots) > 32: raise ValueError("Too many Steam libraries")
    for library in approved:
        candidate=library/"steamapps"
        if candidate not in roots: roots.append(candidate)
        if len(roots)>32: raise ValueError("Too many Steam libraries")
    return roots, sorted(set(warnings))


def installed(manifest, home, external=()):
    identity = app_id(Path(manifest).stem.removeprefix("appmanifest_"))
    data = read_vdf(manifest, home, 65536, external).get("AppState")
    if not isinstance(data, dict) or data.get("appid") != identity: raise ValueError("Steam manifest identity changed")
    if data.get("StateFlags") != "4": return None
    title, folder = data.get("name"), data.get("installdir")
    if (not isinstance(title, str) or title != title.strip() or not title or len(title) > 256
        or any(ord(c) < 32 for c in title) or not isinstance(folder, str) or not folder
        or len(folder) > 256 or "\\" in folder or Path(folder).is_absolute()
        or any(p in (".", "..") for p in folder.split("/"))):
        raise ValueError("Invalid installed Steam title metadata")
    common = private_path(Path(manifest).parent/"common", home, True, external)
    game = private_path(common/folder, home, True, external)
    if not game.is_relative_to(common) or game == common: raise ValueError("Redirected Steam game folder")
    return {"schemaVersion":1, "title":title, "platform":"Steam", "runtimeId":"steam", "root":"", "target":identity}


def discover(home, external=()):
    roots, warnings = libraries(home, external)
    games = {}
    entries = 0
    for root in roots:
        with os.scandir(root) as iterator:
            for entry in iterator:
                entries += 1
                if entries > 10000: raise ValueError("Steam discovery exceeds 10,000 entries; nothing was imported")
                if not re.fullmatch(r"appmanifest_[1-9][0-9]{0,9}\.acf", entry.name): continue
                game = installed(Path(entry.path), home, external)
                if game:
                    if game["target"] in games: raise ValueError("Duplicate Steam game identity across libraries")
                    games[game["target"]] = game
                    if len(games) > MAX_GAMES: raise ValueError("Steam discovery exceeds 5,000 games")
    return {"games":sorted(games.values(), key=lambda g:(g["title"].casefold(), g["target"])), "warnings":warnings}


def verify_installed(home, identity, external=()):
    roots, _ = libraries(home, external)
    matches = []
    for root in roots:
        manifest = root/("appmanifest_"+app_id(identity)+".acf")
        if manifest.exists():
            game = installed(manifest, home, external)
            if game: matches.append(game)
    if len(matches) != 1:
        raise ValueError("This Steam game is missing, updating or ambiguous. Check installation in Steam, then retry")
    return matches[0]


def matching_processes(variable, value, proc=Path("/proc"), token=None, exclude_client=False):
    result = []
    entries = 0
    for entry in proc.iterdir():
        if not entry.name.isdigit() or int(entry.name) == os.getpid(): continue
        entries += 1
        if entries > 4096: raise RuntimeError("Steam session process limit exceeded")
        try:
            with (entry/"environ").open("rb") as stream: environment = stream.read(65537)
            if len(environment) > 65536: continue
            fields = environment.split(b"\0")
            if token is not None and ("ISOLDE_STEAM_SESSION="+token).encode() not in fields: continue
            if exclude_client:
                executable = os.path.basename(os.readlink(entry/"exe")).lower()
                if executable in ("steam", "steam.sh", "steamwebhelper", "steamservice", "steam-runtime-launcher-service"):
                    continue
            if (variable+"="+value).encode() in fields:
                if (entry/"stat").read_text().rsplit(") ",1)[1].split()[0] != "Z": result.append(int(entry.name))
        except (OSError, ValueError, IndexError): continue
    return result


def game_processes(identity, proc=Path("/proc"), token=None):
    # Both names occur in Steam/Proton environments. Neither is entitlement
    # proof, and a tagged third-party launcher may still precede the game.
    return sorted(set(matching_processes("SteamAppId",identity,proc,token,True)
                      + matching_processes("SteamGameId",identity,proc,token,True)))


def signal_owned_processes(pids, token):
    # A unique inherited environment token, in addition to the private PID
    # namespace, prevents shutdown commands from reaching an existing client.
    for pid in pids:
        if pid<=1: continue
        descriptor=None
        try:
            descriptor=os.pidfd_open(pid)
            with (Path("/proc")/str(pid)/"environ").open("rb") as stream: environment=stream.read(65537)
            if ("ISOLDE_STEAM_SESSION="+token).encode() in environment.split(b"\0"):
                signal.pidfd_send_signal(descriptor,signal.SIGTERM)
        except (ProcessLookupError,FileNotFoundError): pass
        finally:
            if descriptor is not None:os.close(descriptor)


def wait_until_gone(processes, timeout, clock=time.monotonic, sleep=time.sleep):
    deadline=clock()+timeout
    while clock()<deadline:
        if not processes(): return True
        sleep(.05)
    return not processes()


def stop_owned_session(token):
    signal_owned_processes(matching_processes("ISOLDE_STEAM_SESSION",token), token)
    deadline=time.monotonic()+3
    while time.monotonic()<deadline:
        if not matching_processes("ISOLDE_STEAM_SESSION",token):return
        time.sleep(.05)


def wait_for_game(identity, launcher, timeout=120, clock=time.monotonic, sleep=time.sleep, processes=game_processes):
    deadline = clock()+timeout
    while clock() < deadline:
        if processes(identity): return
        if launcher.poll() not in (None, 0): raise RuntimeError("Steam could not start this game. Check the active account, ownership, connection and updates in Steam, then retry")
        sleep(.25)
    raise RuntimeError("Steam did not start this game within two minutes. Check the active account, offline availability, installation and compatibility in Steam, then retry")


def launch(identity, external=()):
    app_id(identity)
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE") != "1": raise ValueError("Steam games require an isolated Isolde session")
    home = Path.home()
    verify_installed(home, identity, external)
    binary = Path("/usr/games/steam")
    if not binary.is_file() or not os.access(binary, os.X_OK): raise ValueError("Install or repair Steam in the Isolde Store")
    # HOME, /tmp, IPC, PID view and bus all belong to NativeSandbox.
    def stopped(signum, frame): raise SessionCanceled()
    previous_term = signal.signal(signal.SIGTERM, stopped)
    previous_int = signal.signal(signal.SIGINT, stopped)
    token=uuid.uuid4().hex
    processes = lambda game: game_processes(game, token=token)
    started = False
    canceled = False
    try:
        launcher = subprocess.Popen([str(binary), "-silent", "-applaunch", identity], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    env=dict(os.environ,ISOLDE_STEAM_SESSION=token))
        wait_for_game(identity, launcher, processes=processes)
        started = True
        print("STEAM_GAME_PROCESS_OBSERVED "+identity, file=sys.stderr, flush=True)
        missing = None
        while True:
            if processes(identity): missing = None
            elif missing is None: missing = time.monotonic()
            elif time.monotonic()-missing >= 3: break
            time.sleep(.25)
    except SessionCanceled:
        canceled = True
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            if canceled:
                signal_owned_processes(processes(identity), token)
                stopped = wait_until_gone(lambda: processes(identity), 5)
            else:
                stopped = started
            # Give Steam a bounded opportunity to process game exit and sync before
            # ending its private client. This is not a cloud-sync confirmation.
            if stopped:
                time.sleep(3)
            stop_owned_session(token)
        finally:
            signal.signal(signal.SIGTERM, previous_term)
            signal.signal(signal.SIGINT, previous_int)
        # No global Steam IPC shutdown. NativeSandbox teardown is the final
        # boundary for descendants that discard the token. Cloud sync is not
        # promised by this direct-launch adapter.


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--discover", action="store_true")
    group.add_argument("--launch")
    parser.add_argument("--home", type=Path)
    parser.add_argument("--library", action="append", default=[])
    args = parser.parse_args()
    if args.discover:
        if args.home is None: parser.error("A private Steam home is required")
        result = discover(args.home,args.library)
        data = json.dumps(result, ensure_ascii=False)
        if len(data.encode()) > 2*1024*1024: raise ValueError("Steam import preview is too large")
        print(data)
    else:
        if args.home is not None: parser.error("Launch uses the isolated profile home only")
        launch(args.launch,args.library)


if __name__ == "__main__":
    try: main()
    except (OSError, ValueError, RuntimeError) as error:
        print("Steam game: "+str(error), file=sys.stderr, flush=True)
        sys.exit(2)
