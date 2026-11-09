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
import zipfile

MAX_ENTRIES = 10000
MAX_EXPANDED = 2 * 1024 * 1024 * 1024
MAX_ARCHIVE = 512 * 1024 * 1024
MANIFEST = "isolde-game.json"


def inspect(source):
    source = Path(source)
    if source.is_symlink():
        raise ValueError("Choose a regular portable game ZIP no larger than 512 MiB.")
    descriptor=os.open(source,os.O_RDONLY|getattr(os,"O_NOFOLLOW",0)|getattr(os,"O_NONBLOCK",0))
    if not stat.S_ISREG(os.fstat(descriptor).st_mode) or os.fstat(descriptor).st_size>MAX_ARCHIVE:
        os.close(descriptor)
        raise ValueError("Choose a regular portable game ZIP no larger than 512 MiB.")
    with os.fdopen(descriptor,"rb") as stream:
        digest=hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024*1024), b""):
            digest.update(chunk)
        stream.seek(0)
        with zipfile.ZipFile(stream) as archive:
            infos = archive.infolist()
            if not infos or len(infos) > MAX_ENTRIES:
                raise ValueError("Portable game bundle has too many or too few files.")
            total = 0
            names = set()
            for info in infos:
                path = PurePosixPath(info.filename)
                mode = (info.external_attr >> 16) & 0xFFFF
                file_type = stat.S_IFMT(mode)
                canonical_name = "/".join(path.parts) + ("/" if info.is_dir() else "")
                if (not info.filename or path.is_absolute() or "\\" in info.filename
                        or "\x00" in info.filename or info.filename != canonical_name
                        or any(part in ("", ".", "..") for part in path.parts)
                        or path.parts[0] == ".isolde-install.json"
                        or info.filename in names or info.flag_bits & 1
                        or file_type not in (0, stat.S_IFREG, stat.S_IFDIR)):
                    raise ValueError("Portable game bundle contains an unsafe or duplicate path.")
                names.add(info.filename)
                if info.file_size < 0 or info.file_size > MAX_EXPANDED:
                    raise ValueError("Portable game file exceeds the unpacked size limit.")
                if not info.is_dir() and info.compress_size > MAX_ARCHIVE:
                    raise ValueError("Portable game member exceeds the compressed size limit.")
                total += info.file_size
                if total > MAX_EXPANDED:
                    raise ValueError("Portable game bundle exceeds the 2 GiB unpacked size limit.")
            try:
                manifest_info=archive.getinfo(MANIFEST)
                if manifest_info.file_size>65536:raise ValueError("Portable game manifest exceeds its 64 KiB limit.")
                manifest = json.loads(archive.read(manifest_info))
            except (KeyError, UnicodeDecodeError, json.JSONDecodeError):
                raise ValueError("Bundle must contain a valid isolde-game.json manifest.") from None
            if (not isinstance(manifest, dict) or set(manifest) != {"schemaVersion", "title", "platform", "entrypoint"}
                    or manifest["schemaVersion"] != 1
                    or not isinstance(manifest["title"], str) or not manifest["title"].strip()
                    or len(manifest["title"]) > 256 or any(ord(c) < 32 for c in manifest["title"])
                    or not isinstance(manifest["platform"], str) or not manifest["platform"].strip()
                    or len(manifest["platform"]) > 80
                    or not isinstance(manifest["entrypoint"], str)):
                raise ValueError("Portable game manifest has invalid fields.")
            entry = PurePosixPath(manifest["entrypoint"])
            if (entry.is_absolute() or "\\" in manifest["entrypoint"]
                    or any(part in ("", ".", "..") for part in entry.parts)
                    or manifest["entrypoint"] not in names
                    or archive.getinfo(manifest["entrypoint"]).is_dir()):
                raise ValueError("Portable game entry point must name a file inside the bundle.")
            entry_info=archive.getinfo(manifest["entrypoint"])
            if entry_info.file_size > MAX_ARCHIVE:raise ValueError("Game entry point exceeds the 512 MiB executable limit.")
            with archive.open(entry_info) as entry_stream:executable=entry_stream.read(20)
            if not executable.startswith(b"\x7fELF") or len(executable) < 20:
                raise ValueError("Only native Linux ELF game entry points are supported by this installer.")
            return {"schemaVersion": 1, "title": manifest["title"].strip(),
                    "platform": manifest["platform"].strip(), "entrypoint": manifest["entrypoint"],
                    "entrySize": len(executable), "archiveEntries": len(infos), "unpackedBytes": total,
                    "archiveSha256": digest.hexdigest()}


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
            raise ValueError("Choose a regular portable game ZIP.")
        source_fd = os.open(source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        if not stat.S_ISREG(os.fstat(source_fd).st_mode):
            os.close(source_fd)
            raise ValueError("Choose a regular portable game ZIP.")
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
                if copied_archive>MAX_ARCHIVE:raise ValueError("Choose a regular portable game ZIP no larger than 512 MiB.")
                saved.write(chunk)
            saved.flush();os.fsync(saved.fileno())
        metadata=inspect(snapshot)
        if expected_sha256 and metadata["archiveSha256"]!=expected_sha256:
            raise ValueError("The package changed after preflight. Review it again before installing.")
        with zipfile.ZipFile(snapshot) as archive:
            written=0
            for info in archive.infolist():
                relative = PurePosixPath(info.filename)
                target = stage.joinpath(*relative.parts)
                if info.is_dir():
                    target.mkdir(mode=0o700, parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
                if hasattr(os, "O_NOFOLLOW"):
                    flags |= os.O_NOFOLLOW
                descriptor = os.open(target, flags, 0o600)
                copied=0
                mode=(info.external_attr>>16)&0o111
                with os.fdopen(descriptor, "wb") as output, archive.open(info) as input_file:
                    while True:
                        chunk=input_file.read(1024*1024)
                        if not chunk:break
                        copied+=len(chunk);written+=len(chunk)
                        if copied>info.file_size or written>MAX_EXPANDED:
                            raise ValueError("Portable game archive expanded beyond its declared size.")
                        output.write(chunk)
                    if copied!=info.file_size:raise ValueError("Portable game archive member size changed.")
                    output.flush()
                    os.fsync(output.fileno())
                if mode:target.chmod(0o700)
        entry = stage.joinpath(*PurePosixPath(metadata["entrypoint"]).parts)
        if (entry.is_symlink() or not entry.is_file()
                or entry.resolve(strict=True) != entry.absolute()
                or entry.stat().st_size != archive.getinfo(metadata["entrypoint"]).file_size):
            raise ValueError("Installed entry point failed destination verification.")
        entry.chmod(0o700)
        with entry.open("rb") as installed_entry:
            if installed_entry.read(20)[:4] != b"\x7fELF":
                raise ValueError("Installed entry point failed ELF verification.")
        marker = stage / ".isolde-install.json"
        with marker.open("w",encoding="utf-8") as marker_file:
            marker_file.write(json.dumps(metadata, ensure_ascii=False))
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


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--inspect", action="store_true")
    group.add_argument("--install", action="store_true")
    parser.add_argument("--source", required=True)
    parser.add_argument("--destination")
    parser.add_argument("--expected-sha256")
    args = parser.parse_args()
    if args.inspect:
        result = inspect(args.source)
        print(json.dumps(result, ensure_ascii=False))
    else:
        if not args.destination:
            parser.error("--install requires --destination")
        install(args.source, args.destination,args.expected_sha256)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        print("Portable game install: " + str(error), file=sys.stderr)
        sys.exit(2)
