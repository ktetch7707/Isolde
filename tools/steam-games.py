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


def private_path(path, home, directory=False):
    home = Path(home).absolute()
    if home.resolve() != home:
        raise ValueError("Steam profile storage is redirected")
    path = Path(path)
    canonical = path.resolve(strict=True)
    if not canonical.is_relative_to(home) or canonical == home:
        raise ValueError("Steam library is outside this Isolde profile")
    if directory and not canonical.is_dir(): raise ValueError("Steam library folder is unavailable")
    return canonical


def read_vdf(path, home, limit):
    path = Path(path)
    if path.is_symlink(): raise ValueError("Redirected Steam manifest")
    path = private_path(path, home)
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


def libraries(home):
    home = Path(home).absolute()
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
        folders = read_vdf(index, home, 1024*1024).get("libraryfolders", {})
        if not isinstance(folders, dict) or len(folders) > 32: raise ValueError("Too many Steam libraries")
        for number, entry in folders.items():
            if not number.isdigit(): continue
            name = entry.get("path") if isinstance(entry, dict) else entry
            if not isinstance(name, str) or not Path(name).is_absolute(): raise ValueError("Invalid Steam library path")
            candidate = Path(name)/"steamapps"
            if not candidate.exists(): continue
            try: candidate = private_path(candidate, home, True)
            except ValueError:
                warnings.append("External Steam libraries are not connected to this Isolde profile."); continue
            if candidate not in roots: roots.append(candidate)
            if len(roots) > 32: raise ValueError("Too many Steam libraries")
    return roots, sorted(set(warnings))


def installed(manifest, home):
    identity = app_id(Path(manifest).stem.removeprefix("appmanifest_"))
    data = read_vdf(manifest, home, 65536).get("AppState")
    if not isinstance(data, dict) or data.get("appid") != identity: raise ValueError("Steam manifest identity changed")
    if data.get("StateFlags") != "4": return None
    title, folder = data.get("name"), data.get("installdir")
    if (not isinstance(title, str) or title != title.strip() or not title or len(title) > 256
        or any(ord(c) < 32 for c in title) or not isinstance(folder, str) or not folder
        or len(folder) > 256 or "\\" in folder or Path(folder).is_absolute()
        or any(p in (".", "..") for p in folder.split("/"))):
        raise ValueError("Invalid installed Steam title metadata")
    common = private_path(Path(manifest).parent/"common", home, True)
    game = private_path(common/folder, home, True)
    if not game.is_relative_to(common) or game == common: raise ValueError("Redirected Steam game folder")
    return {"schemaVersion":1, "title":title, "platform":"Steam", "runtimeId":"steam", "root":"", "target":identity}


def discover(home):
    roots, warnings = libraries(home)
    games = {}
    entries = 0
    for root in roots:
        with os.scandir(root) as iterator:
            for entry in iterator:
                entries += 1
                if entries > 10000: raise ValueError("Steam discovery exceeds 10,000 entries; nothing was imported")
                if not re.fullmatch(r"appmanifest_[1-9][0-9]{0,9}\.acf", entry.name): continue
                game = installed(Path(entry.path), home)
                if game:
                    if game["target"] in games: raise ValueError("Duplicate Steam game identity across libraries")
                    games[game["target"]] = game
                    if len(games) > MAX_GAMES: raise ValueError("Steam discovery exceeds 5,000 games")
    return {"games":sorted(games.values(), key=lambda g:(g["title"].casefold(), g["target"])), "warnings":warnings}


def verify_installed(home, identity):
    roots, _ = libraries(home)
    matches = []
    for root in roots:
        manifest = root/("appmanifest_"+app_id(identity)+".acf")
        if manifest.exists():
            game = installed(manifest, home)
            if game: matches.append(game)
    if len(matches) != 1:
        raise ValueError("This Steam game is missing, updating or ambiguous. Check installation in Steam, then retry")
    return matches[0]


def matching_processes(variable, value, proc=Path("/proc")):
    result = []
    entries = 0
    for entry in proc.iterdir():
        if not entry.name.isdigit() or int(entry.name) == os.getpid(): continue
        entries += 1
        if entries > 4096: raise RuntimeError("Steam session process limit exceeded")
        try:
            with (entry/"environ").open("rb") as stream: environment = stream.read(65537)
            if len(environment) > 65536: continue
            if (variable+"="+value).encode() in environment.split(b"\0"):
                if (entry/"stat").read_text().rsplit(") ",1)[1].split()[0] != "Z": result.append(int(entry.name))
        except (OSError, ValueError, IndexError): continue
    return result


def game_processes(identity, proc=Path("/proc")):
    return matching_processes("SteamAppId",identity,proc)


def stop_owned_session(token):
    # A unique inherited environment token, in addition to the private PID
    # namespace, prevents shutdown commands from reaching an existing client.
    for pid in matching_processes("ISOLDE_STEAM_SESSION",token):
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
    deadline=time.monotonic()+1.5
    while time.monotonic()<deadline:
        if not matching_processes("ISOLDE_STEAM_SESSION",token):return
        time.sleep(.05)


def wait_for_game(identity, launcher, timeout=120, clock=time.monotonic, sleep=time.sleep, processes=game_processes):
    deadline = clock()+timeout
    while clock() < deadline:
        if processes(identity): return
        if launcher.poll() not in (None, 0): raise RuntimeError("Steam could not start this game. Check sign-in, ownership and updates in Steam")
        sleep(.25)
    raise RuntimeError("Steam did not start this game within two minutes. Check sign-in, installation and compatibility, then retry")


def launch(identity):
    app_id(identity)
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE") != "1": raise ValueError("Steam games require an isolated Isolde session")
    home = Path.home()
    verify_installed(home, identity)
    binary = Path("/usr/games/steam")
    if not binary.is_file() or not os.access(binary, os.X_OK): raise ValueError("Install or repair Steam in the Isolde Store")
    # HOME, /tmp, IPC, PID view and bus all belong to NativeSandbox.
    def stopped(signum, frame): raise InterruptedError("Steam game launch canceled")
    signal.signal(signal.SIGTERM, stopped)
    signal.signal(signal.SIGINT, stopped)
    token=uuid.uuid4().hex
    launcher = subprocess.Popen([str(binary), "-silent", "-applaunch", identity], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                env=dict(os.environ,ISOLDE_STEAM_SESSION=token))
    try:
        wait_for_game(identity, launcher)
        print("STEAM_GAME_STARTED "+identity, file=sys.stderr, flush=True)
        missing = None
        while True:
            if game_processes(identity): missing = None
            elif missing is None: missing = time.monotonic()
            elif time.monotonic()-missing >= 3: break
            time.sleep(.25)
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        stop_owned_session(token)
        # No global Steam IPC shutdown. NativeSandbox teardown is the final
        # boundary for descendants that discard the token. Cloud sync is not
        # promised by this direct-launch adapter.


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--discover", action="store_true")
    group.add_argument("--launch")
    parser.add_argument("--home", type=Path)
    args = parser.parse_args()
    if args.discover:
        if args.home is None: parser.error("A private Steam home is required")
        result = discover(args.home)
        data = json.dumps(result, ensure_ascii=False)
        if len(data.encode()) > 2*1024*1024: raise ValueError("Steam import preview is too large")
        print(data)
    else:
        if args.home is not None: parser.error("Launch uses the isolated profile home only")
        launch(args.launch)


if __name__ == "__main__":
    try: main()
    except (OSError, ValueError, RuntimeError) as error:
        print("Steam game: "+str(error), file=sys.stderr, flush=True)
        sys.exit(2)
