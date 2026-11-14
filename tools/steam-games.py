#!/usr/bin/env python3
"""Read this Isolde profile's installed Steam titles; supervise direct launches.

No Web API, credential reads, provider writes, custom launch options or shell.
Steam owns authentication, downloads, compatibility, game files and saves.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import signal
import socket
import struct
import stat
import subprocess
import sys
import time
import uuid
from urllib.parse import unquote

MAX_GAMES = 5000
FACT_VALUES = {
    "process": {"unknown", "waiting", "observed", "gone"},
    "installation": {"unknown", "verified", "unavailable"},
    "client": {"unknown", "available", "unavailable"},
    "ownership": {"unknown", "owned", "not_owned"},
    "account": {"unknown", "signed_in", "signed_out"},
    "connection": {"unknown", "online", "offline"},
    "offline": {"unknown", "ready", "unavailable"},
    "exit": {"unknown", "exited", "crashed"},
    "local_save": {"unknown", "flushed", "failed"},
    "cloud_save": {"unknown", "synced", "pending", "failed"},
    "session_end": {"unknown", "normal", "canceled", "incomplete", "recovery_required"},
    "error": {"none", "bootstrap_failed", "ownership_denied", "auth_required",
              "offline_unavailable", "launch_failed", "install_check_failed", "client_unavailable",
              "process_observation_failed"},
}
UNKNOWN_FACTS = {key: ("waiting" if key == "process" else "none" if key == "error" else "unknown")
                 for key in FACT_VALUES}
MAX_RECOVERY_GAMES = 64
MAX_RECOVERY_BYTES = 64 * 1024
STEAM_CLIENT_EXECUTABLES = {
    "steam", "steam.sh", "steamwebhelper", "steamservice", "steam-runtime-launcher-service",
}


class SessionCanceled(Exception):
    pass


class ProcessObservationError(RuntimeError):
    """The private process view was incomplete; absence cannot be inferred."""


def open_recovery_directory(home):
    home = Path(home).absolute()
    if home.resolve(strict=True) != home or not home.is_dir():
        raise ValueError("Steam recovery storage is redirected or unavailable")
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
    try:
        descriptor = os.open(home, flags)
    except OSError as error:
        raise ValueError("Steam recovery storage is redirected or unavailable") from error
    for part in (".local", "state", "isolde"):
        try:
            os.mkdir(part, mode=0o700, dir_fd=descriptor)
        except FileExistsError:
            pass
        try:
            child = os.open(part, flags, dir_fd=descriptor)
        except OSError as error:
            os.close(descriptor)
            raise ValueError("Steam recovery storage is redirected or unavailable") from error
        os.close(descriptor)
        descriptor = child
    return descriptor


def recovery_directory(home):
    descriptor = open_recovery_directory(home)
    os.close(descriptor)
    return Path(home).absolute()/".local/state/isolde"


def load_recovery_journal(home):
    directory_fd = open_recovery_directory(home)
    try:
        try:
            descriptor = os.open("steam-recovery.json", os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                                 dir_fd=directory_fd)
        except FileNotFoundError:
            return {}
        except OSError as error:
            raise ValueError("Steam recovery record is redirected or unavailable") from error
        with os.fdopen(descriptor, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise ValueError("Steam recovery record is not a regular file")
            raw = stream.read(MAX_RECOVERY_BYTES + 1)
    finally:
        os.close(directory_fd)
    if len(raw) > MAX_RECOVERY_BYTES:
        raise ValueError("Steam recovery record exceeds its size limit")
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate Steam recovery record field")
            result[key] = value
        return result
    try:
        journal = json.loads(raw, object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("Invalid Steam recovery record") from error
    if not isinstance(journal, dict) or set(journal) != {"version", "games"} \
            or journal["version"] != 1 or not isinstance(journal["games"], dict) \
            or len(journal["games"]) > MAX_RECOVERY_GAMES:
        raise ValueError("Invalid Steam recovery record")
    for identity, record in journal["games"].items():
        app_id(identity)
        if not isinstance(record, dict) or record.get("appId") != identity \
                or record.get("version") != 2 or record.get("session_end") not in {"incomplete", "recovery_required"} \
                or set(record) != set(FACT_VALUES) | {"version", "appId", "phase", "recordedAt"} \
                or record.get("phase") not in {"starting", "process_observed", "ended", "failed", "canceled"} \
                or not isinstance(record.get("recordedAt"), str) or len(record["recordedAt"]) > 32:
            raise ValueError("Invalid Steam recovery record")
        validated_facts({key: record[key] for key in FACT_VALUES})
    return journal["games"]


def write_recovery_status(home, identity, facts, phase="failed"):
    app_id(identity)
    facts = validated_facts(facts)
    if facts["session_end"] not in {"incomplete", "recovery_required"}:
        raise ValueError("Steam recovery status is not pending")
    games = load_recovery_journal(home)
    if identity not in games and len(games) >= MAX_RECOVERY_GAMES:
        raise ValueError("Steam recovery record limit reached; review existing Steam recovery warnings")
    record = {"version": 2, "appId": identity, "phase": phase, **facts,
              "recordedAt": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    games[identity] = record
    payload = json.dumps({"version": 1, "games": games}, separators=(",", ":"), sort_keys=True).encode()
    if len(payload) > MAX_RECOVERY_BYTES:
        raise ValueError("Steam recovery record exceeds its size limit")
    directory_fd = open_recovery_directory(home)
    temporary = ".steam-recovery-" + uuid.uuid4().hex
    try:
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory_fd)
        with os.fdopen(descriptor, "wb") as stream:
            if stream.write(payload) != len(payload):
                raise OSError("Short Steam recovery record write")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, "steam-recovery.json", src_dir_fd=directory_fd, dst_dir_fd=directory_fd)
        os.fsync(directory_fd)
    finally:
        try:
            os.unlink(temporary, dir_fd=directory_fd)
        except FileNotFoundError:
            pass
        os.close(directory_fd)


def clear_recovery_status(home, identity):
    app_id(identity)
    games = load_recovery_journal(home)
    if identity not in games:
        return False
    del games[identity]
    directory_fd = open_recovery_directory(home)
    temporary = ".steam-recovery-" + uuid.uuid4().hex
    payload = json.dumps({"version": 1, "games": games}, separators=(",", ":"), sort_keys=True).encode()
    try:
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                             0o600, dir_fd=directory_fd)
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, "steam-recovery.json", src_dir_fd=directory_fd, dst_dir_fd=directory_fd)
        os.fsync(directory_fd)
    finally:
        try:
            os.unlink(temporary, dir_fd=directory_fd)
        except FileNotFoundError:
            pass
        os.close(directory_fd)
    return True


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


def private_bus_processes():
    """Identify the private bus server by kernel credentials, never its name."""
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE")!="1": return set()
    address=os.environ.get("DBUS_SESSION_BUS_ADDRESS","")
    if not address.startswith("unix:") or ";" in address: return set()
    try:
        fields=dict(part.split("=",1) for part in address[5:].split(","))
        path=unquote(fields.get("path",""))
        if not path.startswith(("/tmp/",f"/run/user/{os.getuid()}/")) or len(path.encode())>104 or "\0" in path:return set()
        with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as client:
            client.settimeout(.25);client.connect(path)
            pid,uid,_=struct.unpack("3i",client.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
        return {pid} if pid>0 and uid==os.getuid() else set()
    except (OSError,ValueError,struct.error):return set()


def matching_processes(variable, value, proc=Path("/proc"), token=None, exclude_client=False):
    result = []
    entries = 0
    # The protected native host supervises this helper; it is not a game child.
    # Its environment is deliberately unreadable after PR_SET_DUMPABLE(0).
    ancestors=set()
    ancestor=os.getpid()
    for _ in range(128):
        if ancestor<=0 or ancestor in ancestors: break
        ancestors.add(ancestor)
        try:
            directory=proc/str(ancestor)
            if directory.stat().st_uid!=os.getuid(): break
            ancestor=int((directory/"stat").read_text().rsplit(") ",1)[1].split()[1])
        except (OSError,ValueError,IndexError): break
    excluded=ancestors | private_bus_processes()
    try:
        for entry in proc.iterdir():
            if not entry.name.isdigit() or int(entry.name) in excluded: continue
            entries += 1
            if entries > 4096: raise ProcessObservationError("Steam session process limit exceeded")
            try:
                if entry.stat().st_uid != os.getuid(): continue
                if (entry/"stat").read_text().rsplit(") ",1)[1].split()[0] in ("Z","X","x"): continue
                with (entry/"environ").open("rb") as stream: environment = stream.read(65537)
                if len(environment) > 65536:
                    raise ProcessObservationError("Steam session process metadata is oversized")
                fields = environment.split(b"\0")
                if token is not None and ("ISOLDE_STEAM_SESSION="+token).encode() not in fields: continue
                if exclude_client:
                    executable = os.path.basename(os.readlink(entry/"exe")).lower()
                    if executable in ("steam", "steam.sh", "steamwebhelper", "steamservice", "steam-runtime-launcher-service"):
                        continue
                if (variable+"="+value).encode() in fields:
                    if (entry/"stat").read_text().rsplit(") ",1)[1].split()[0] not in ("Z","X","x"): result.append(int(entry.name))
            except (FileNotFoundError, ProcessLookupError):
                # Processes can exit between /proc enumeration and inspection.
                continue
            except OSError as error:
                try:
                    if (entry/"stat").read_text().rsplit(") ",1)[1].split()[0] in ("Z","X","x"): continue
                except (FileNotFoundError,ProcessLookupError): continue
                except (OSError,ValueError,IndexError): pass
                raise ProcessObservationError("Steam session process state is not fully observable") from error
            except (ValueError, IndexError) as error:
                raise ProcessObservationError("Steam session process state is malformed") from error
    except ProcessObservationError:
        raise
    except OSError as error:
        raise ProcessObservationError("Steam session process list is not observable") from error
    return result


def game_processes(identity, proc=Path("/proc"), token=None):
    # Both names occur in Steam/Proton environments. Neither is entitlement
    # proof, and a tagged third-party launcher may still precede the game.
    return sorted(set(matching_processes("SteamAppId",identity,proc,token,True)
                      + matching_processes("SteamGameId",identity,proc,token,True)))


def signal_owned_processes(pids, token, identity):
    # Never signal a process based on a remembered PID alone. Re-open a pidfd
    # and revalidate both the launch token and selected App ID immediately
    # before signaling; Steam clients are left to private namespace teardown.
    pids=list(pids)
    if not pids:
        return
    if not hasattr(os,"pidfd_open") or not hasattr(signal,"pidfd_send_signal"):
        raise ProcessObservationError("This system cannot safely target the Steam game process")
    for pid in pids:
        if pid<=1: continue
        descriptor=None
        try:
            descriptor=os.pidfd_open(pid)
            with (Path("/proc")/str(pid)/"environ").open("rb") as stream: environment=stream.read(65537)
            if len(environment)>65536:
                raise ProcessObservationError("Steam game process metadata is oversized")
            fields=environment.split(b"\0")
            if ("ISOLDE_STEAM_SESSION="+token).encode() not in fields:
                raise ProcessObservationError("Steam game process ownership changed before termination")
            if not any((name+"="+identity).encode() in fields for name in ("SteamAppId","SteamGameId")):
                raise ProcessObservationError("Steam game identity changed before termination")
            executable=os.path.basename(os.readlink(Path("/proc")/str(pid)/"exe")).lower()
            if executable in STEAM_CLIENT_EXECUTABLES:
                raise ProcessObservationError("Refusing to signal the Steam client as a game process")
            signal.pidfd_send_signal(descriptor,signal.SIGTERM)
        except (ProcessLookupError,FileNotFoundError): pass
        except OSError as error:
            raise ProcessObservationError("Steam session ownership could not be verified") from error
        finally:
            if descriptor is not None:os.close(descriptor)


def wait_until_gone(processes, timeout, clock=time.monotonic, sleep=time.sleep):
    deadline=clock()+timeout
    while clock()<deadline:
        if not processes(): return True
        sleep(.05)
    return not processes()


class SteamProcessProvider:
    """Local process boundary; it has no authenticated Steam account API."""
    def __init__(self, identity, token, launcher, preflight=None):
        self.identity, self.token, self.launcher = identity, token, launcher
        self.preflight = dict(preflight or UNKNOWN_FACTS)

    def observe(self):
        facts = dict(self.preflight)
        try:
            facts["process"] = "observed" if game_processes(self.identity, token=self.token) else "gone"
        except ProcessObservationError:
            facts["process"] = "unknown"
            facts["error"] = "process_observation_failed"
        if self.launcher.poll() not in (None, 0): facts["error"] = "bootstrap_failed"
        return facts


def validated_facts(value):
    if not isinstance(value, dict) or set(value) != set(FACT_VALUES):
        raise ValueError("Invalid Steam provider observation")
    if any(not isinstance(value[k], str) or value[k] not in allowed for k, allowed in FACT_VALUES.items()):
        raise ValueError("Invalid Steam provider observation")
    return dict(value)


def provider_message(error):
    return {
        "bootstrap_failed": "Steam could not start this game. Check the active account, ownership, connection and updates in Steam, then retry",
        "ownership_denied": "Steam reported that this account cannot launch this game. Check the active account and ownership in Steam, then retry",
        "auth_required": "Steam requires account sign-in before this game can start. Open Steam and sign in, then retry",
        "offline_unavailable": "Steam reported that this game is unavailable offline. Reconnect in Steam and prepare the game for offline use, then retry",
        "launch_failed": "Steam reported that this game could not start. Check installation and compatibility in Steam, then retry",
        "install_check_failed": "The Steam installation could not be verified. Check the game and library access in Steam, then retry",
        "client_unavailable": "Install or repair Steam in the Isolde Store",
        "process_observation_failed": "Isolde could not reliably observe this Steam session. Return Home and check Steam and saves before retrying",
    }[error]


def supervise_provider(provider, publish, timeout=120, clock=None, sleep=None):
    """Consume only bounded provider observations; absence is never a crash or save proof."""
    clock = clock or time.monotonic
    sleep = sleep or time.sleep
    deadline = clock()+timeout
    seen = False
    missing = None
    last = None
    while True:
        facts = validated_facts(provider.observe())
        if facts["process"] == "unknown" and facts["error"] == "none":
            facts["error"] = "process_observation_failed"
        phase = "process_observed" if seen or facts["process"] == "observed" else "starting"
        if facts != last:
            publish(phase, facts)
            last = facts
        if facts["error"] != "none" and (not seen or facts["error"] != "bootstrap_failed"):
            raise RuntimeError(provider_message(facts["error"]))
        if facts["process"] == "observed":
            seen = True
            missing = None
        elif seen:
            if missing is None: missing = clock()
            elif clock()-missing >= 3:
                if facts["exit"] == "crashed":
                    facts["session_end"] = "recovery_required"
                    publish("failed", facts)
                    raise RuntimeError("Steam reported that this game crashed. Check its saves and compatibility before retrying")
                if facts["local_save"] == "failed":
                    facts["session_end"] = "recovery_required"
                    publish("failed", facts)
                    raise RuntimeError("Steam reported that the game could not save. Check local saves before retrying")
                if facts["cloud_save"] == "failed":
                    facts["session_end"] = "recovery_required"
                    publish("failed", facts)
                    raise RuntimeError("Steam reported that Cloud sync failed. Check Steam before using these saves on another device")
                facts["session_end"] = "normal" if facts["exit"] == "exited" else "unknown"
                publish("ended", facts)
                return facts
        elif clock() >= deadline:
            facts["error"] = "launch_failed"
            publish("failed", facts)
            raise RuntimeError("Steam did not start this game within two minutes. Check the active account, offline availability, installation and compatibility in Steam, then retry")
        sleep(.25)


def publish_provider_state(identity, phase, facts):
    payload = {"version":2, "appId":identity, "phase":phase, **facts}
    print("STEAM_PROVIDER_STATE "+json.dumps(payload, separators=(",", ":"), sort_keys=True),
          file=sys.stderr, flush=True)


def launch(identity, external=()):
    app_id(identity)
    if os.environ.get("ISOLDE_NATIVE_NAMESPACE") != "1": raise ValueError("Steam games require an isolated Isolde session")
    preflight = dict(UNKNOWN_FACTS)
    publish_provider_state(identity, "starting", preflight)
    home = Path.home()
    previous_recovery=load_recovery_journal(home).get(identity)
    try:
        verify_installed(home, identity, external)
    except (OSError, ValueError):
        preflight["installation"] = "unavailable"
        preflight["error"] = "install_check_failed"
        publish_provider_state(identity, "failed", preflight)
        raise
    preflight["installation"] = "verified"
    binary = Path("/usr/games/steam")
    if not binary.is_file() or not os.access(binary, os.X_OK):
        preflight["client"] = "unavailable"
        preflight["error"] = "client_unavailable"
        publish_provider_state(identity, "failed", preflight)
        raise ValueError("Install or repair Steam in the Isolde Store")
    preflight["client"] = "available"
    publish_provider_state(identity, "starting", preflight)
    # HOME, /tmp, IPC, PID view and bus all belong to NativeSandbox.
    def stopped(signum, frame): raise SessionCanceled()
    previous_term = signal.signal(signal.SIGTERM, stopped)
    previous_int = signal.signal(signal.SIGINT, stopped)
    token=uuid.uuid4().hex
    processes = lambda game: game_processes(game, token=token)
    started = False
    canceled = False
    clean_game_exit = True
    cleanup_complete = True
    marker_written = False
    last_facts = dict(preflight)
    failed = False
    def publish(phase, facts):
        nonlocal started, last_facts, marker_written
        last_facts = facts
        if phase == "process_observed" and not started:
            started = True
            print("STEAM_GAME_PROCESS_OBSERVED "+identity, file=sys.stderr, flush=True)
        if marker_written:
            journal_facts=dict(facts,session_end="incomplete")
            write_recovery_status(home,identity,journal_facts,phase)
        publish_provider_state(identity, phase, facts)
    try:
        initial_recovery=dict(preflight,session_end="incomplete")
        write_recovery_status(home,identity,initial_recovery,"starting")
        marker_written = True
        launcher = subprocess.Popen([str(binary), "-silent", "-applaunch", identity], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                    env=dict(os.environ,ISOLDE_STEAM_SESSION=token))
        supervise_provider(SteamProcessProvider(identity, token, launcher, preflight), publish)
    except SessionCanceled:
        canceled = True
    except (OSError, ValueError, RuntimeError):
        failed = True
        raise
    finally:
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            game_was_running = started
            if canceled or failed:
                try:
                    owned_games=processes(identity)
                    game_was_running = game_was_running or bool(owned_games)
                    signal_owned_processes(owned_games, token, identity)
                    clean_game_exit = wait_until_gone(lambda: processes(identity), 5)
                except ProcessObservationError as error:
                    print("Steam session observation: "+str(error),file=sys.stderr,flush=True)
                    clean_game_exit = False
                    cleanup_complete = False
                    last_facts["process"] = "unknown"
                    last_facts["error"] = "process_observation_failed"
                stopped = clean_game_exit
            else:
                stopped = started
            # Give Steam a bounded opportunity to process game exit and sync before
            # ending its private client. This is not a cloud-sync confirmation.
            if stopped and game_was_running:
                time.sleep(3)
            if canceled or failed:
                if started and clean_game_exit:
                    last_facts["process"] = "gone"
                if not clean_game_exit or not cleanup_complete:
                    last_facts["session_end"] = "incomplete"
                elif canceled and game_was_running:
                    last_facts["session_end"] = "incomplete"
                elif canceled:
                    last_facts["session_end"] = "canceled"
                publish("failed" if failed or not clean_game_exit else "canceled", last_facts)
            if marker_written:
                try:
                    if last_facts["session_end"] in ("incomplete","recovery_required"):
                        write_recovery_status(home,identity,last_facts,"failed" if failed else "canceled")
                    elif previous_recovery is not None:
                        previous_facts={key:previous_recovery[key] for key in FACT_VALUES}
                        write_recovery_status(home,identity,previous_facts,previous_recovery["phase"])
                    else:
                        clear_recovery_status(home,identity)
                except (OSError,ValueError,RuntimeError) as error:
                    cleanup_complete=False
                    last_facts["session_end"]="incomplete"
                    publish_provider_state(identity,"failed",last_facts)
                    print("Steam recovery status: could not safely update the durable warning: "+str(error),
                          file=sys.stderr,flush=True)
        finally:
            signal.signal(signal.SIGTERM, previous_term)
            signal.signal(signal.SIGINT, previous_int)
        # Never signal Steam client/helper PIDs by session token alone.
        # Exiting this isolated PID namespace tears down its remaining client
        # descendants; no global Steam IPC shutdown is issued.
    if not clean_game_exit or not cleanup_complete:
        raise RuntimeError("The Steam game session could not be fully observed or stopped. Recent saves or Cloud sync may be incomplete; open Steam to check before retrying")


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
