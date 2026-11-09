#!/usr/bin/env python3
"""Install bounded, inert Isolde portable-game bundles; never execute archive content."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import signal
import tarfile
import zipfile
import uuid

MAX_ENTRIES = 10000
MAX_EXPANDED = 2 * 1024 * 1024 * 1024
MAX_ARCHIVE = 512 * 1024 * 1024
MANIFEST = "isolde-game.json"


class StrictTarInfo(tarfile.TarInfo):
    def _proc_pax(self, archive):
        raise ValueError("Extended tar headers are not supported.")

    def _proc_gnulong(self, archive):
        raise ValueError("Extended tar headers are not supported.")

    def _proc_sparse(self, archive):
        raise ValueError("Sparse tar entries are not supported.")


def open_bundle(stream, archive_format):
    stream.seek(0)
    if archive_format == "zip":
        return zipfile.ZipFile(stream)
    return tarfile.open(fileobj=stream, mode="r:gz" if archive_format == "ustar-gzip" else "r:",
                        tarinfo=StrictTarInfo)


def bundle_format(stream):
    if zipfile.is_zipfile(stream):
        stream.seek(0)
        return "zip"
    stream.seek(0)
    if stream.read(2) == b"\x1f\x8b":
        stream.seek(0)
        return "ustar-gzip"
    stream.seek(257)
    if stream.read(5) == b"ustar":
        stream.seek(0)
        return "ustar"
    raise ValueError("Choose a ZIP, USTAR or gzip-compressed USTAR game bundle.")


def bundle_members(archive, archive_format):
    members = []
    total = 0
    names = set()
    for info in archive.infolist() if archive_format == "zip" else archive:
        if len(members) >= MAX_ENTRIES:
            raise ValueError("Portable game bundle has too many or too few entries.")
        if archive_format == "zip":
            raw_name = info.filename
            directory = info.is_dir()
            size = info.file_size
            mode = (info.external_attr >> 16) & 0xFFFF
            file_type = stat.S_IFMT(mode)
            invalid_type = file_type not in (0, stat.S_IFREG, stat.S_IFDIR)
            invalid_encryption = bool(info.flag_bits & 1)
            compressed_size = info.compress_size
        else:
            raw_name = info.name
            directory = info.isdir()
            size = info.size
            mode = info.mode
            invalid_type = not (directory or info.isfile()) or bool(info.pax_headers)
            invalid_encryption = False
            compressed_size = size
        name = raw_name.rstrip("/") if directory and archive_format!="zip" else (
            raw_name[:-1] if directory and raw_name.endswith("/") else raw_name)
        path = PurePosixPath(name)
        canonical_name = "/".join(path.parts) + ("/" if directory else "")
        canonical_match=raw_name==canonical_name or (
            archive_format!="zip" and directory and raw_name==name)
        if (not name or not path.parts or path.is_absolute() or "\\" in name or "\x00" in name
                or len(name.encode("utf-8", "surrogatepass")) > 4096
                or not canonical_match or any(part in ("", ".", "..") for part in path.parts)
                or path.parts[0] == ".isolde-install.json" or invalid_type
                or invalid_encryption or size < 0 or compressed_size > MAX_ARCHIVE):
            raise ValueError("Portable game bundle contains an unsafe or non-canonical entry.")
        if name in names:
            raise ValueError("Portable game bundle contains an unsafe or duplicate path.")
        names.add(name)
        total += size
        if size > MAX_EXPANDED or total > MAX_EXPANDED:
            raise ValueError("Portable game bundle exceeds the 2 GiB unpacked size limit.")
        members.append({"name": name, "directory": directory, "size": size,
                        "mode": mode, "info": info})
    return members


def open_member(archive, member, archive_format):
    if archive_format == "zip":
        return archive.open(member["info"])
    result = archive.extractfile(member["info"])
    if result is None:
        raise ValueError("Portable game archive contains an unreadable file.")
    return result


def inspect(source):
    source = Path(source)
    if source.is_symlink():
        raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
    descriptor=os.open(source,os.O_RDONLY|getattr(os,"O_NOFOLLOW",0)|getattr(os,"O_NONBLOCK",0))
    if not stat.S_ISREG(os.fstat(descriptor).st_mode) or os.fstat(descriptor).st_size>MAX_ARCHIVE:
        os.close(descriptor)
        raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
    with os.fdopen(descriptor,"rb") as stream:
        digest=hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024*1024), b""):
            digest.update(chunk)
        archive_format=bundle_format(stream)
        with open_bundle(stream,archive_format) as archive:
            members=bundle_members(archive,archive_format)
            if not members or len(members) > MAX_ENTRIES:
                raise ValueError("Portable game bundle has too many or too few entries.")
            total = sum(member["size"] for member in members)
            names={member["name"] for member in members}
            manifest_members=[member for member in members if member["name"]==MANIFEST]
            if len(manifest_members)!=1 or manifest_members[0]["directory"]:
                raise ValueError("Bundle must contain a valid isolde-game.json manifest.")
            manifest_member=manifest_members[0]
            if manifest_member["size"]>65536:
                raise ValueError("Portable game manifest exceeds its 64 KiB limit.")
            try:
                with open_member(archive,manifest_member,archive_format) as manifest_stream:
                    manifest=json.loads(manifest_stream.read(65537))
            except (UnicodeDecodeError, json.JSONDecodeError):
                raise ValueError("Bundle must contain a valid isolde-game.json manifest.") from None
            if (not isinstance(manifest, dict) or set(manifest) != {"schemaVersion", "title", "platform", "entrypoint"}
                    or type(manifest["schemaVersion"]) is not int or manifest["schemaVersion"] != 1
                    or not isinstance(manifest["title"], str) or not manifest["title"].strip()
                    or len(manifest["title"]) > 256 or any(ord(c) < 32 for c in manifest["title"])
                    or not isinstance(manifest["platform"], str) or not manifest["platform"].strip()
                    or len(manifest["platform"]) > 80
                    or any(ord(c)<32 for c in manifest["platform"])
                    or not isinstance(manifest["entrypoint"], str)):
                raise ValueError("Portable game manifest has invalid fields.")
            entry = PurePosixPath(manifest["entrypoint"])
            if (entry.is_absolute() or "\\" in manifest["entrypoint"]
                    or any(part in ("", ".", "..") for part in entry.parts)
                    or manifest["entrypoint"] not in names
                    or next(member for member in members if member["name"]==manifest["entrypoint"])["directory"]):
                raise ValueError("Portable game entry point must name a file inside the bundle.")
            entry_info=next(member for member in members if member["name"]==manifest["entrypoint"])
            if entry_info["size"] > MAX_ARCHIVE:raise ValueError("Game entry point exceeds the 512 MiB executable limit.")
            with open_member(archive,entry_info,archive_format) as entry_stream:executable=entry_stream.read(20)
            if not executable.startswith(b"\x7fELF") or len(executable) < 20:
                raise ValueError("Only native Linux ELF game entry points are supported by this installer.")
            return {"schemaVersion": 1, "title": manifest["title"].strip(),
                    "platform": manifest["platform"].strip(), "entrypoint": manifest["entrypoint"],
                    "entrySize": entry_info["size"], "archiveEntries": len(members), "unpackedBytes": total,
                    "archiveSha256": digest.hexdigest(),"archiveFormat":archive_format}


def install(source, destination, expected_sha256=None):
    source = Path(source)
    destination = Path(destination)
    stage = destination.with_name(destination.name + ".staging")
    snapshot = destination.with_name(destination.name + ".staging-source")
    if (destination.exists() or destination.is_symlink() or stage.exists() or stage.is_symlink()
            or snapshot.exists() or snapshot.is_symlink()):
        raise ValueError("Install destination already exists. Existing files will not be overwritten.")
    destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if destination.parent.is_symlink() or destination.parent.resolve() != destination.parent.absolute():
        raise ValueError("Install destination parent must not be a symlink.")
    stage.mkdir(mode=0o700)
    def interrupted(signum, frame):
        raise InterruptedError("Installation canceled")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        if source.is_symlink() or not source.is_file():
            raise ValueError("Choose a regular portable game bundle.")
        source_fd = os.open(source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        if not stat.S_ISREG(os.fstat(source_fd).st_mode):
            os.close(source_fd)
            raise ValueError("Choose a regular portable game bundle.")
        try:
            snapshot_fd = os.open(snapshot, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600)
        except Exception:
            os.close(source_fd)
            raise
        with os.fdopen(source_fd, "rb") as input_file, os.fdopen(snapshot_fd, "wb") as saved:
            copied_archive=0
            while True:
                chunk=input_file.read(1024*1024)
                if not chunk:break
                copied_archive+=len(chunk)
                if copied_archive>MAX_ARCHIVE:raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
                saved.write(chunk)
            saved.flush();os.fsync(saved.fileno())
        metadata=inspect(snapshot)
        if expected_sha256 and metadata["archiveSha256"]!=expected_sha256:
            raise ValueError("The package changed after preflight. Review it again before installing.")
        with snapshot.open("rb") as source_stream, open_bundle(source_stream,metadata["archiveFormat"]) as archive:
            members=bundle_members(archive,metadata["archiveFormat"])
            written=0
            for member in members:
                relative = PurePosixPath(member["name"])
                target = stage.joinpath(*relative.parts)
                if member["directory"]:
                    target.mkdir(mode=0o700, parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
                if hasattr(os, "O_NOFOLLOW"):
                    flags |= os.O_NOFOLLOW
                descriptor = os.open(target, flags, 0o600)
                copied=0
                mode=member["mode"]&0o111
                with os.fdopen(descriptor, "wb") as output, open_member(archive,member,metadata["archiveFormat"]) as input_file:
                    while True:
                        chunk=input_file.read(1024*1024)
                        if not chunk:break
                        copied+=len(chunk);written+=len(chunk)
                        if copied>member["size"] or written>MAX_EXPANDED:
                            raise ValueError("Portable game archive expanded beyond its declared size.")
                        output.write(chunk)
                    if copied!=member["size"]:raise ValueError("Portable game archive member size changed.")
                    output.flush()
                    os.fsync(output.fileno())
                if mode:target.chmod(0o700)
        entry = stage.joinpath(*PurePosixPath(metadata["entrypoint"]).parts)
        if (entry.is_symlink() or not entry.is_file()
                or entry.resolve(strict=True) != entry.absolute()
                or entry.stat().st_size != metadata["entrySize"]):
            raise ValueError("Installed entry point failed destination verification.")
        entry.chmod(0o700)
        with entry.open("rb") as installed_entry:
            if installed_entry.read(20)[:4] != b"\x7fELF":
                raise ValueError("Installed entry point failed ELF verification.")
        owned_files = []
        owned_directories = []
        for path in stage.rglob("*"):
            relative = path.relative_to(stage).as_posix()
            if path.is_symlink():
                raise ValueError("Install staging unexpectedly contains a symbolic link.")
            if path.is_dir():
                owned_directories.append(relative)
            elif path.is_file():
                file_digest = hashlib.sha256()
                with path.open("rb") as installed_file:
                    for chunk in iter(lambda: installed_file.read(1024 * 1024), b""):
                        file_digest.update(chunk)
                owned_files.append({"path": relative, "size": path.stat().st_size,
                                    "sha256": file_digest.hexdigest()})
            else:
                raise ValueError("Install staging contains an unsupported file type.")
        marker = stage / ".isolde-install.json"
        with marker.open("w",encoding="utf-8") as marker_file:
            marker_file.write(json.dumps({**metadata, "ownershipSchemaVersion": 1,
                                          "ownedFiles": owned_files,
                                          "ownedDirectories": owned_directories},
                                         ensure_ascii=False))
            marker_file.flush();os.fsync(marker_file.fileno())
        os.chmod(marker, 0o600)
        for directory in sorted((path for path in stage.rglob("*") if path.is_dir()),key=lambda path:len(path.parts),reverse=True)+[stage]:
            descriptor=os.open(directory,os.O_RDONLY|getattr(os,"O_DIRECTORY",0))
            try:os.fsync(descriptor)
            finally:os.close(descriptor)
        os.replace(stage, destination)
        descriptor=os.open(destination.parent,os.O_RDONLY|getattr(os,"O_DIRECTORY",0))
        try:os.fsync(descriptor)
        finally:os.close(descriptor)
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    finally:
        try:snapshot.unlink()
        except FileNotFoundError:pass
    print(json.dumps(metadata, ensure_ascii=False))


def uninstall(root):
    def interrupted(signum, frame):
        raise InterruptedError("Uninstallation canceled")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    root = Path(root)
    if (root.name != "install" or root.is_symlink() or not root.is_dir()
            or root.resolve(strict=True) != root.absolute()):
        raise ValueError("Choose a verified Isolde-managed install folder.")
    marker = root / ".isolde-install.json"
    if (marker.is_symlink() or not marker.is_file() or marker.resolve(strict=True) != marker.absolute()
            or marker.stat().st_size > 16 * 1024 * 1024):
        raise ValueError("The install ownership marker is missing or redirected; files were preserved.")
    try:
        ownership = json.loads(marker.read_text(encoding="utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        raise ValueError("The install ownership marker is invalid; files were preserved.") from None
    files = ownership.get("ownedFiles") if isinstance(ownership, dict) else None
    directories = ownership.get("ownedDirectories") if isinstance(ownership, dict) else None
    if (not isinstance(ownership, dict) or ownership.get("ownershipSchemaVersion") != 1
            or not isinstance(files, list) or not isinstance(directories, list)
            or len(files) > MAX_ENTRIES or len(directories) > MAX_ENTRIES):
        raise ValueError("The install ownership manifest is incomplete; files were preserved.")
    expected_files = {}
    for item in files:
        if not isinstance(item, dict) or set(item) != {"path", "size", "sha256"}:
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        name, size, digest = item["path"], item["size"], item["sha256"]
        path = PurePosixPath(name) if isinstance(name, str) else PurePosixPath(".")
        if (not isinstance(name, str) or not name or path.is_absolute() or "\\" in name
                or any(part in ("", ".", "..") for part in path.parts)
                or name == ".isolde-install.json" or name in expected_files
                or type(size) is not int or size < 0
                or not isinstance(digest, str) or len(digest) != 64
                or any(ch not in "0123456789abcdef" for ch in digest)):
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        expected_files[name] = (size, digest)
    expected_directories = set()
    for name in directories:
        path = PurePosixPath(name) if isinstance(name, str) else PurePosixPath(".")
        if (not isinstance(name, str) or not name or path.is_absolute() or "\\" in name
                or any(part in ("", ".", "..") for part in path.parts)
                or name in expected_directories):
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        expected_directories.add(name)
    actual_files = set()
    actual_directories = set()
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            raise ValueError("The install contains a symbolic link; files were preserved.")
        if path == marker:
            continue
        if path.is_dir():
            actual_directories.add(relative)
        elif path.is_file():
            actual_files.add(relative)
        else:
            raise ValueError("The install contains an unsupported file type; files were preserved.")
    if actual_files != set(expected_files) or actual_directories != expected_directories:
        raise ValueError("The install contains missing or unowned files; nothing was deleted.")
    for name, (size, expected_digest) in expected_files.items():
        path = root.joinpath(*PurePosixPath(name).parts)
        digest = hashlib.sha256()
        with path.open("rb") as installed_file:
            for chunk in iter(lambda: installed_file.read(1024 * 1024), b""):
                digest.update(chunk)
        if path.stat().st_size != size or digest.hexdigest() != expected_digest:
            raise ValueError("An installed file changed after installation; nothing was deleted.")
    if ".isolde-install.json" not in {path.name for path in root.iterdir()}:
        raise ValueError("The install ownership marker changed; files were preserved.")
    quarantine = root.with_name(".isolde-uninstalling-" + str(uuid.uuid4()))
    os.replace(root, quarantine)
    try:
        shutil.rmtree(quarantine)
    except Exception:
        if not root.exists() and quarantine.exists():
            os.replace(quarantine, root)
        raise
    print(json.dumps({"schemaVersion": 1, "removed": True}))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--inspect", action="store_true")
    group.add_argument("--install", action="store_true")
    group.add_argument("--uninstall", action="store_true")
    parser.add_argument("--source")
    parser.add_argument("--destination")
    parser.add_argument("--root")
    parser.add_argument("--expected-sha256")
    args = parser.parse_args()
    if args.inspect:
        if not args.source:
            parser.error("--inspect requires --source")
        result = inspect(args.source)
        print(json.dumps(result, ensure_ascii=False))
    elif args.uninstall:
        if not args.root:
            parser.error("--uninstall requires --root")
        uninstall(args.root)
    else:
        if not args.destination or not args.source:
            parser.error("--install requires --source and --destination")
        install(args.source, args.destination,args.expected_sha256)


if __name__ == "__main__":
    try:
        main()
    except (OSError, EOFError, ValueError, zipfile.BadZipFile, tarfile.TarError, RuntimeError) as error:
        print("Portable game install: " + str(error), file=sys.stderr)
        sys.exit(2)
